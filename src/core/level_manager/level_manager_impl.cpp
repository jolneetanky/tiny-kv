#include "core/level_manager/level_manager_impl.h"
#include <iostream>
#include <filesystem>
#include <unordered_set>
#include <fstream>
#include <queue>
#include "common/log.h"

class LevelManagerImpl::CompactionMergingIterator final : public tinykv::Iterator
{
public:
    explicit CompactionMergingIterator(std::vector<MergeInput> inputs)
        : m_inputs{std::move(inputs)}, m_heap{HeapCompare{&m_inputs}}
    {
        SeekToFirst();
    }

    bool Valid() const override
    {
        return m_valid;
    }

    void SeekToFirst() override
    {
        m_heap = Heap{HeapCompare{&m_inputs}}; // pq
        m_valid = false;

        // for all inputs (one input is one SSTable file)
        // move the iterator to the start of the file
        // ASSUME: input iterators produce keys in SORTED, ASCENDING order.
        for (std::size_t i = 0; i < m_inputs.size(); ++i)
        {
            m_inputs[i].iterator->SeekToFirst();
            if (m_inputs[i].iterator->Valid())
                m_heap.push(i);
        }

        _selectNext();
    }

    void SeekToLast() override
    {
        m_heap = Heap{HeapCompare{&m_inputs}};
        m_valid = false;

        std::optional<std::size_t> winner;
        for (std::size_t i = 0; i < m_inputs.size(); ++i)
        {
            m_inputs[i].iterator->SeekToLast();
            if (!m_inputs[i].iterator->Valid())
                continue;

            if (!winner || _isBetterLastEntry(i, *winner))
                winner = i;
        }

        if (!winner)
            return;

        const auto &lastIterator = *m_inputs[*winner].iterator;
        m_currentEntry = Entry{lastIterator.Key(), lastIterator.Value(), lastIterator.isTombstone()};
        m_valid = true;
    }

    void Seek(const std::string &target) override
    {
        m_heap = Heap{HeapCompare{&m_inputs}};
        m_valid = false;

        for (std::size_t i = 0; i < m_inputs.size(); ++i)
        {
            m_inputs[i].iterator->Seek(target);
            if (m_inputs[i].iterator->Valid())
                m_heap.push(i);
        }

        _selectNext();
    }

    void Next() override
    {
        if (!m_valid)
            return;

        _selectNext();
    }

    const std::string &Key() const override
    {
        return m_currentEntry.key;
    }

    const std::string &Value() const override
    {
        return m_currentEntry.val;
    }

    bool isTombstone() const override
    {
        return m_currentEntry.tombstone;
    }

private:
    struct HeapCompare
    {
        const std::vector<MergeInput> *inputs; // pointer to vector

        // return `true` if `leftIndex` has LOWER PRIORITY than `rightIndex`
        bool operator()(std::size_t leftIndex, std::size_t rightIndex) const
        {
            const MergeInput &left = (*inputs)[leftIndex];
            const MergeInput &right = (*inputs)[rightIndex];
            const std::string &leftKey = left.iterator->Key();
            const std::string &rightKey = right.iterator->Key();

            if (leftKey != rightKey)
                return leftKey > rightKey; // min-heap by key
            if (left.isSourceLevel != right.isSourceLevel)
                return !left.isSourceLevel; // source wins equal keys
            if (left.fileNumber != right.fileNumber)
                // both files belong to the same level (eg. comoparing 2 files in the same level)
                // which MAY contain the same key, as for level 0, but not the other levels
                // in which case, the newer file wins
                return left.fileNumber < right.fileNumber; // newer file wins
            return false;
        }
    };

    using Heap = std::priority_queue<std::size_t, std::vector<std::size_t>, HeapCompare>;

    std::vector<MergeInput> m_inputs;
    Heap m_heap;
    Entry m_currentEntry;
    bool m_valid = false;

    bool _hasHigherPrecedence(std::size_t leftIndex, std::size_t rightIndex) const
    {
        const MergeInput &left = m_inputs[leftIndex];
        const MergeInput &right = m_inputs[rightIndex];
        if (left.isSourceLevel != right.isSourceLevel)
            return left.isSourceLevel;
        return left.fileNumber > right.fileNumber;
    }

    bool _isBetterLastEntry(std::size_t candidateIndex, std::size_t currentIndex) const
    {
        const auto &candidate = *m_inputs[candidateIndex].iterator;
        const auto &current = *m_inputs[currentIndex].iterator;
        if (candidate.Key() != current.Key())
            return candidate.Key() > current.Key();
        return _hasHigherPrecedence(candidateIndex, currentIndex);
    }

    void _selectNext()
    {
        m_valid = false;
        if (m_heap.empty())
            return;

        const std::string smallestKey = m_inputs[m_heap.top()].iterator->Key();

        // handle duplicate keys in other streams -- we only want to keep one!
        // gather every heap entry at the smallest key
        // emit only the highest priority version
        // advance all iterators that had that key
        // then reinsert them
        // IMPT ASSUMPTION: within the same input iterator stream, there are NO duplicate keys.
        std::vector<std::size_t> sameKeyInputs;

        while (!m_heap.empty() && m_inputs[m_heap.top()].iterator->Key() == smallestKey)
        {
            sameKeyInputs.push_back(m_heap.top());
            m_heap.pop();
        }

        // The first entry popped is already the highest-precedence stream
        // for this key, as defined by HeapCompare.
        const std::size_t winner = sameKeyInputs.front();

        const auto &winnerIterator = *m_inputs[winner].iterator;
        m_currentEntry = Entry{winnerIterator.Key(), winnerIterator.Value(), winnerIterator.isTombstone()};

        // advance the iterators for the inputs at the same key
        for (std::size_t inputIndex : sameKeyInputs)
        {
            m_inputs[inputIndex].iterator->Next();
            if (m_inputs[inputIndex].iterator->Valid())
                m_heap.push(inputIndex);
        }

        m_valid = true;
    }
};

LevelManagerImpl::LevelManagerImpl(int levelNum, std::string directoryPath, SystemContext &systemContext, const TableFormat &tableFormat) : m_levelNum{levelNum}, m_directoryPath{directoryPath}, m_systemContext{systemContext}, m_tableFormat{tableFormat}, m_allowOverlap{levelNum == 0}, m_logPrefix{"[LevelManagerImpl::LEVEL_" + std::to_string(levelNum) + "]"} {};

const int &LevelManagerImpl::getLevel()
{
    return m_levelNum;
}

size_t LevelManagerImpl::tableCount() const
{
    return m_tableReaders.size();
}

std::vector<SSTableMetadata> LevelManagerImpl::tableMetadata() const
{
    std::vector<SSTableMetadata> metadata;
    metadata.reserve(m_tableReaders.size());
    for (const auto &tableReader : m_tableReaders)
        metadata.emplace_back(tableReader->meta());
    return metadata;
}

std::vector<std::shared_ptr<const TableReader>> LevelManagerImpl::tableReadersByFileNumber(
    const std::vector<FileNumber> &fileNumbers) const
{
    std::vector<std::shared_ptr<const TableReader>> readers;
    readers.reserve(fileNumbers.size());

    for (FileNumber fileNumber : fileNumbers)
    {
        const auto tableIt = std::find_if(
            m_tableReaders.begin(),
            m_tableReaders.end(),
            [fileNumber](const auto &reader)
            {
                return reader->meta().m_file_number == fileNumber;
            });
        if (tableIt != m_tableReaders.end())
            readers.emplace_back(*tableIt);
    }

    return readers;
}

std::string LevelManagerImpl::_generateSSTableFileName() const
{
    static std::atomic<uint64_t> counter{0};

    auto now = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                  now.time_since_epoch())
                  .count();

    uint64_t uniqueId = (ns << 16) ^ counter.fetch_add(1); // mix counter + time

    return "table-" + std::to_string(uniqueId);
}

TimestampType LevelManagerImpl::_getTimeNow()
{
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();

    return timestamp;
}

// NEW API
std::optional<Entry> LevelManagerImpl::getKey(const std::string &key) const
{
    TINYKV_LOG("[LevelManagerImpl.getKey()] LEVEL " + std::to_string(m_levelNum) + ", KEY: " + key);

    for (auto tableIt = m_tableReaders.rbegin(); tableIt != m_tableReaders.rend(); ++tableIt)
    {
        const auto &tableReader = *tableIt;
        if (!tableReader->withinRange(key))
        {
            continue;
        }

        std::optional<Entry> entry = tableReader->get(key);
        if (!entry)
        {
            continue;
        }

        if (entry->tombstone) // stop at the first tombstone
        {
            TINYKV_LOG("[LevelManagerImpl.getKey()] FOUND TOMBSTONED KEY " + key);
            return entry;
        }

        TINYKV_LOG("[LevelManagerImpl.getKey()] FOUND KEY " + key);
        return entry;
    }

    TINYKV_LOG("[LevelManagerImpl.getKey()] key \"" + key + "\" does not exist on disk");
    return std::nullopt;
};

// time = O(n), because we insert in front
// but the cost is not significant, as we assume number of SSTables in a level at any one time remains small (thanks to compaction).
Status LevelManagerImpl::createTable(tinykv::Iterator &entries)
{
    TINYKV_LOG("[LevelManagerImpl.createTable()]");

    std::string fullPath = m_directoryPath + "/" + _generateSSTableFileName();
    TimestampType timestamp = _getTimeNow();
    FileNumber file_num = m_systemContext.file_number_allocator.next();

    std::shared_ptr<const TableReader> tableReader = m_tableFormat.writeTable(fullPath, entries, timestamp, file_num);

    m_tableReaders.emplace_back(std::move(tableReader));

    return Status::OK();
};

Status LevelManagerImpl::initNew()
{
    TINYKV_LOG(m_logPrefix + "[init()]");

    // 1. Look through existing files in the directory for this level, and initialize them
    for (const auto &dirEntry : std::filesystem::directory_iterator{m_directoryPath})
    {
        if (!dirEntry.is_regular_file())
        {
            continue;
        }

        std::shared_ptr<const TableReader> tableReader = m_tableFormat.openTable(dirEntry.path().string());
        m_systemContext.file_number_allocator.restore(tableReader->meta().m_file_number); // note the largest file number seen
        m_tableReaders.emplace_back(std::move(tableReader));
    }

    // Directory iteration order is unspecified. File numbers are allocated
    // monotonically and persisted in each table, so restore oldest-first order.
    std::sort(m_tableReaders.begin(), m_tableReaders.end(), [](const auto &left, const auto &right)
              {
                  const SSTableMetadata leftMeta = left->meta();
                  const SSTableMetadata rightMeta = right->meta();
                  if (leftMeta.m_file_number != rightMeta.m_file_number)
                      return leftMeta.m_file_number < rightMeta.m_file_number;
                  if (leftMeta.m_timestamp != rightMeta.m_timestamp)
                      return leftMeta.m_timestamp < rightMeta.m_timestamp;
                  return left->getFilePath() < right->getFilePath(); });

    return Status::OK();
};

// The level doesn't know if it contains overlapping files or not. It also doesn't know if `other` contains overlapping files.
// So just in case, it merges overlapping files for both levels.
// But by right, only level 0 contains overlapping files.
Status LevelManagerImpl::compactInto(LevelManager &destination, const tinykv::CompactionPlan &plan)
{
    const auto sourceTables = tableReadersByFileNumber(plan.sourceFileNumbers);
    if (sourceTables.size() != plan.sourceFileNumbers.size())
        return Status::Error("Compaction plan references a missing source table");

    const auto destinationTables = destination.tableReadersByFileNumber(plan.destinationFileNumbers);
    if (destinationTables.size() != plan.destinationFileNumbers.size())
        return Status::Error("Compaction plan references a missing destination table");

    // merge keys from `sourceTables` to `destinationTables`
    std::unique_ptr<tinykv::Iterator> mergedEntries =
        _mergeCompactionInputs(sourceTables, destinationTables);

    // Persist the replacement before deleting any source files.
    Status status = destination.createTable(*mergedEntries);
    if (!status.ok())
        return status;

    status = deleteTablesByFileNumber(plan.sourceFileNumbers);
    if (!status.ok())
        return status;

    return destination.deleteTablesByFileNumber(plan.destinationFileNumbers);
};

std::unique_ptr<tinykv::Iterator> LevelManagerImpl::_mergeCompactionInputs(
    const std::vector<std::shared_ptr<const TableReader>> &sourceTables,
    const std::vector<std::shared_ptr<const TableReader>> &destinationTables) const
{
    // one `MergeInput` per table
    std::vector<MergeInput> inputs;
    inputs.reserve(sourceTables.size() + destinationTables.size());

    const auto appendTableIterators = [&inputs](
                                          const std::vector<std::shared_ptr<const TableReader>> &tables,
                                          bool isSourceLevel) -> bool
    {
        for (const auto &table : tables)
        {
            try
            {
                std::unique_ptr<tinykv::Iterator> iterator = table->NewSortedIterator();
                if (!iterator)
                    return false;

                inputs.push_back(MergeInput{
                    std::move(iterator),
                    isSourceLevel,
                    table->meta().m_file_number});
            }
            catch (const std::exception &error)
            {
                TINYKV_LOG("[LevelManagerImpl::_mergeCompactionInputs()] Failed to create table iterator: " << error.what());
                return false;
            }
        }

        return true;
    };

    if (!appendTableIterators(sourceTables, true) ||
        !appendTableIterators(destinationTables, false))
    {
        return nullptr;
    }

    return std::make_unique<CompactionMergingIterator>(std::move(inputs));
}

Status LevelManagerImpl::deleteTablesByFileNumber(const std::vector<FileNumber> &fileNumbers)
{
    for (FileNumber fileNumber : fileNumbers)
    {
        const auto tableIt = std::find_if(
            m_tableReaders.begin(),
            m_tableReaders.end(),
            [fileNumber](const auto &reader)
            {
                return reader->meta().m_file_number == fileNumber;
            });
        if (tableIt == m_tableReaders.end())
            return Status::Error("Cannot delete missing table " + std::to_string(fileNumber));

        std::error_code ec;
        const std::string filePath = (*tableIt)->getFilePath();
        if (filePath.empty())
            return Status::Error("Table reader has no backing file path");

        const bool removed = std::filesystem::remove(filePath, ec);
        if (ec)
            return Status::Error("Failed to delete table " + filePath + ": " + ec.message());
        if (!removed)
            return Status::Error("Table file does not exist: " + filePath);

        m_tableReaders.erase(tableIt);
    }

    return Status::OK();
}

#if 0
// HELPER FUNCTIONS
// TODO: implement
Status LevelManagerImpl::_mergeOverlappingTables()
{
    TINYKV_LOG("LevelManagerImpl::_mergeOverlappingTables()");

    if (m_tableReaders.size() == 0)
        return Status::OK();
    // ASSUME: tables are alr in sorted order (by file number)
    // group tables with overlapping key ranges tgt
    // then for each of these groups

    // sort by startKey
    std::vector<const SSTable *> tables; // stores pointer to this level's tables, sorted based on start keys
    tables.reserve(this->m_tableReaders.size());

    for (auto &tableHandle : this->m_tableReaders)
        tables.push_back(tableHandle.table.get());

    std::sort(tables.begin(), tables.end(), [](const SSTable *t1, const SSTable *t2)
              { return t1->getStartKey() < t2->getStartKey(); });

    // std::string intvStart = tables[0]->getStartKey();

    // 1. MERGE
    std::string intvEnd = tables[0]->getEndKey();

    int startIdx = 0;
    int endIdx = 0;

    std::vector<std::pair<int, int>> overlappingTables;

    for (int i = 0; i < tables.size(); i++)
    {
        auto &table = tables[i];
        if (table->getStartKey() > intvEnd)
        {
            // non-overlapping
            overlappingTables.emplace_back(startIdx, endIdx);
            startIdx = i;
            endIdx = i;
        }
        else
        {
            // overlapping
            endIdx = i;
            std::string endKey = tables[i]->getEndKey();
            if (endKey > intvEnd)
                intvEnd = endKey;
        }
    }

    overlappingTables.emplace_back(startIdx, endIdx);

    // 2. MERGE OVERLAPPING TABLES TGT
    // for each overlapping interval, merge the entries, then write a new file, then delete the corresponding tables
    for (auto &[start, end] : overlappingTables)
    {
        if (start == end)
            continue;

        // 1. Collect victim tables
        std::vector<const SSTable *> victims;
        for (int i = start; i <= end; ++i)
            victims.push_back(tables[i]);

        // sort based on ascending order
        std::sort(victims.begin(), victims.end(), [](const SSTable *t1, const SSTable *t2)
                  { return t2->meta() < t1->meta(); });

        // 2. Merge entries (newer tables override older ones)
        // ASSUME: m_ssTables is ordered newest-first
        std::unordered_set<Entry> merged;
        std::vector<Entry> mergedEntries;

        for (const SSTable *table : victims)
        {
            for (const Entry &e : table->getEntries())
            {
                // insert only if key not seen yet
                // newer tables win because we iterate newest → oldest
                // if (merged.find(e.key) == merged.end())
                if (!merged.count(e))
                {
                    merged.emplace(e);
                    mergedEntries.push_back(e);
                }
            }
        }

        // 4. Create new SSTable
        Status s = createTable(std::move(mergedEntries));
        if (!s.ok())
            return s;

        // 5. Delete old SSTables
        s = _deleteTables(victims);
        if (!s.ok())
            return s;
    }

    return Status::OK();
};

Status LevelManagerImpl::_deleteTables(std::vector<const SSTable *> &tables)
{
    if (tables.empty())
    {
        return Status::OK();
    }

    // unordered set of pointers for fast lookup
    std::unordered_set<const SSTable *> victims(tables.begin(), tables.end());

    auto it = m_tableReaders.begin();
    while (it != m_tableReaders.end())
    {
        if (victims.count(it->table.get()))
        {
            it = m_tableReaders.erase(it);
        }
        else
        {
            it++;
        }
    }

    return Status::OK();
};
#endif
