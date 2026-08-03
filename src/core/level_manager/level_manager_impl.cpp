#include "core/level_manager/level_manager_impl.h"
#include <iostream>
#include <filesystem>
#include <unordered_set>
#include <fstream>
#include "common/log.h"

LevelManagerImpl::LevelManagerImpl(int levelNum, std::string directoryPath, SystemContext &systemContext, const TableFormat &tableFormat) : m_levelNum{levelNum}, m_directoryPath{directoryPath}, m_systemContext{systemContext}, m_tableFormat{tableFormat}, m_allowOverlap{levelNum == 0}, m_logPrefix{"[LevelManagerImpl::LEVEL_" + std::to_string(levelNum) + "]"} {};

const int &LevelManagerImpl::getLevel()
{
    return m_levelNum;
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

    for (const auto &tableReader : m_tableReaders)
    {
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
            break;
        }

        TINYKV_LOG("[LevelManagerImpl.getKey()] FOUND KEY " + key);
        return entry;
    }

    TINYKV_LOG("[LevelManagerImpl.getKey()] key \"" + key + "\" does not exist on disk");
    return std::nullopt;
};

// time = O(n), because we insert in front
// but the cost is not significant, as we assume number of SSTables in a level at any one time remains small (thanks to compaction).
// TODO: change to an iterator of Entry
Status LevelManagerImpl::createTable(tinykv::Iterator &entries)
{
    TINYKV_LOG("[LevelManagerImpl.createTable()]");

    std::string full_path = m_directoryPath + "/" + _generateSSTableFileName();
    TimestampType timestamp = _getTimeNow();
    FileNumber file_num = m_systemContext.file_number_allocator.next();

    std::shared_ptr<const TableReader> tableReader = m_tableFormat.writeTable(full_path, entries, timestamp, file_num);

    m_tableReaders.insert(m_tableReaders.begin(), std::move(tableReader));

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
        m_tableReaders.emplace_back(std::move(tableReader));
    }

    return Status::OK();
};

// The level doesn't know if it contains overlapping files or not. It also doesn't know if `other` contains overlapping files.
// So just in case, it merges overlapping files for both levels.
// But by right, only level 0 contains overlapping files.
Status LevelManagerImpl::compactInto(LevelManager &other)
{
    // TODO: Rework compaction against TableReader iterators.
    return Status::OK();
};

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
