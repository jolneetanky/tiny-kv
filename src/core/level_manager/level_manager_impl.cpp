#include "core/level_manager/level_manager_impl.h"
#include "core/sstable_manager/sstable_manager_impl.h"
#include "core/sstable_manager/sstable_writer.h"
#include "core/sstable_manager/sstable_reader.h"
#include <iostream>
#include <filesystem>
#include <unordered_set>
#include <fstream>
#include "common/log.h"

LevelManagerImpl::LevelManagerImpl(int levelNum, std::string directoryPath, SystemContext &systemContext) : m_levelNum{levelNum}, m_directoryPath{directoryPath}, m_systemContext{systemContext}, m_allowOverlap{levelNum == 0} {};

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

/*
1. Creates the directory for this level if it doesn't exist
2. Look through existing files (ie. SSTables) in this level, loads their manager into memory
*/
std::optional<Error> LevelManagerImpl::init()
{
    TINYKV_LOG("[LevelManagerImpl.init()]");

    // 1. Ceate directory if it doesn't exist
    if (!std::filesystem::exists(m_directoryPath))
    {
        try
        {
            if (!std::filesystem::create_directories(m_directoryPath))
            {
                return Error{"Failed to create directory: " + m_directoryPath};
            }
            TINYKV_LOG("[LevelManagerImpl.init()] Created directory: " << m_directoryPath);
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            return Error{std::string("Filesystem error creating directory: ") + e.what()};
        }
    }

    // 2. Look through existing files in the directory for this level, and initialize them
    for (auto const &dirEntry : std::filesystem::directory_iterator{m_directoryPath})
    {
        const std::string &fileName = dirEntry.path().filename().string();

        // read the files
        SSTableReader reader;
        std::unique_ptr<SSTable> table = std::make_unique<SSTable>(reader.read(dirEntry.path().string()));
        m_ssTables.emplace_back(std::move(table)); // emplace the rvalue, which calls the move ctor of SSTable.

        // TODO: delete this
        auto fileManager = std::make_unique<SSTableManagerImpl>(m_directoryPath, fileName, m_systemContext);

        m_ssTableManagers.push_back(std::move(fileManager));
    }

    return std::nullopt;
};

// NEW API
std::optional<Entry> LevelManagerImpl::getKey(const std::string &key) const
{
    TINYKV_LOG("[LevelManagerImpl.getKey()] LEVEL " << std::to_string(m_levelNum));

    // sort SSTables wrt fileNum
    for (const auto &ssTable : m_ssTables)
    {
        if (!ssTable->contains(key))
        {
            continue;
        }

        std::optional<Entry> entryOpt{ssTable->get(key)};
        if (entryOpt)
        {
            TINYKV_LOG("[LevelManagerImpl.getKey()] FOUND");
            return entryOpt;
        }

        // in the latest entry, key has been deleted. So can stop searching alr
        if (entryOpt && entryOpt->tombstone)
        {
            break;
        }
    }

    TINYKV_LOG("[LevelManagerImpl.getKey()] key does not exist on disk");
    return std::nullopt;
};

// time = O(n), because we insert in front
// but the cost is not significant, as we assume number of SSTables in a level at any one time remains small (thanks to compaction).
Status LevelManagerImpl::createTable(std::vector<Entry> &&entries)
{
    TINYKV_LOG("[LevelManagerImpl.createTable()]");

    SSTableWriter writer;
    std::string full_path = m_directoryPath + "/" + _generateSSTableFileName();
    TimestampType timestamp = _getTimeNow();
    FileNumber file_num = m_systemContext.file_number_allocator.next();

    SSTableMetadata metadata = writer.write(full_path, entries, timestamp, file_num);
    std::unique_ptr<SSTable> table = std::make_unique<SSTable>(metadata, std::move(entries));

    m_ssTables.insert(m_ssTables.begin(), std::move(table)); // calls move ctor `unique_ptr(unique_ptr&&)`, same as if we did push_back. No diff here.

    return Status::OK();
};

Status LevelManagerImpl::initNew()
{
    TINYKV_LOG("[LevelManagerImpl.initNew()]");

    // 1. Look through existing files in the directory for this level, and initialize them
    for (const auto &dirEntry : std::filesystem::directory_iterator{m_directoryPath})
    {
        const std::string &fileName = dirEntry.path().filename().string();

        // read the files
        SSTableReader reader;
        std::unique_ptr<SSTable> table = std::make_unique<SSTable>(reader.read(dirEntry.path().string()));
        m_ssTables.emplace_back(std::move(table)); // emplace the rvalue, which calls the move ctor of SSTable.
    }

    return Status::OK();
};

// The level doesn't know if it contains overlapping files or not. It also doesn't know if `other` contains overlapping files.
// So just in case, it merges overlapping files for both levels.
// But by right, only level 0 contains overlapping files.
Status LevelManagerImpl::compactInto(LevelManager &other)
{
    if (m_ssTables.size() == 0)
        return Status::OK();

    // now it's not overlapping
    // look for overlapping file in the other LevelManager
    // for now can just search linearly

    LevelManagerImpl &otherImpl = static_cast<LevelManagerImpl &>(other);

    if (this->m_allowOverlap)
    {
        this->_mergeOverlappingTables();
    }

    if (otherImpl.m_allowOverlap)
    {
        otherImpl._mergeOverlappingTables();
    }

    // 1. find the maximum overlapping interval. Store overlapping level `n` tables in `std::vector<const SSTable *> levelNTables`, same for those in level `other`
    // 2. `vector<Entry> mergedEntries` stores the updated entries, whether older ones have been overwritten.
    // 3. Add every entry in all `levelNTables` to `mergedEntries`, then add every entry in all `otherTables` to `mergedEntries`, but only if they haven't ben seen
    // 4. sort `mergedEntries`, then write to level `other`.

    std::vector<const SSTable *> thisTables; // stores pointer to this level's tables, sorted based on start keys
    thisTables.reserve(this->m_ssTables.size());
    for (auto &ssTable : this->m_ssTables)
        thisTables.push_back(ssTable.get());

    // sort tables based on start key
    std::sort(thisTables.begin(), thisTables.end(), [](const SSTable *t1, const SSTable *t2)
              { return t1->getStartKey() < t2->getStartKey(); });

    // If the `other` level has no files, then this is empty
    std::vector<const SSTable *>
        otherTables;
    otherTables.reserve(otherImpl.m_ssTables.size());
    for (auto &ssTable : otherImpl.m_ssTables)
        otherTables.push_back(ssTable.get());

    std::sort(otherTables.begin(), otherTables.end(), [](const SSTable *t1, const SSTable *t2)
              { return t1->getStartKey() < t2->getStartKey(); });

    // merge tables in this level with tables in `otherLevel` that have overlapping key ranges.
    int thisTableIdx = 0;  // definitely a valid index
    int otherTableIdx = 0; // NOTE: the `other` level might be empty.
    // NOTE: the tables are NOT sorted wrt keys. We cannot assume the same order of start keys!
    // Currently the SSTables are stored based on when they were inserted!

    std::string intvStart = thisTables[0]->getStartKey();
    std::string intvEnd = thisTables[0]->getEndKey();

    // INVARIANTS AT THE START OF EACH LOOP:
    // we are starting from a new interval.
    // `thisTableIdx` points to the index of the current table of the new interval.
    // `otherTableIdx` points to the index of the first other table that is not part of the previous interval, OR to the end index of the other tables.
    //
    while (thisTableIdx < this->m_ssTables.size())
    {
        std::vector<const SSTable *> thisLvlTables;
        std::vector<const SSTable *> otherLvlTables;

        // GOAL:
        // keep adding tables from this level the interval until there is no more overlap
        // after adding a table to this level,, update iinterval end, then add all overlapping tables from `otherTables`

        // INVARIANTS AT THE START OF EACH LOOP:
        // 1. `intvEnd` includes the previous table + all other tables that overlapped with that interval.
        // The current table MAY OR MAY NOT overlap with `intvEnd`.
        // But the very first table at the start of this while loop definitely overlaps.
        while (thisTableIdx < thisTables.size())
        {
            // check `thisTableIdx` if it overlaps
            // not overlapping if:
            // table.end < intvStart || intvEnd < table.start
            // first condition is always false, as we sorted `thisTables` based on start key, and `intvStart` was from a previous table of this level, OR earlier than that
            // meaning `table.end` >= intvStart definitely.
            auto &thisTable = thisTables[thisTableIdx];
            std::string thisStartKey = thisTable->getStartKey();
            std::string thisEndKey = thisTable->getEndKey();

            if (thisStartKey > intvEnd)
            {
                break;
            }

            thisLvlTables.emplace_back(thisTable);

            // update interval end
            if (thisEndKey > intvEnd)
            {
                intvEnd = thisEndKey;
            }

            // now check for overlapping `otherTables`
            while (otherTableIdx < otherTables.size())
            {
                auto &otherTable = otherTables[otherTableIdx];
                std::string otherStartKey = otherTable->getStartKey();
                std::string otherEndKey = otherTable->getEndKey();

                if (otherStartKey > intvEnd)
                {
                    break;
                }
                else
                {
                    otherLvlTables.emplace_back(otherTable);
                    // update interval end
                    if (otherEndKey > intvEnd)
                    {
                        intvEnd = otherEndKey;
                    }
                }

                otherTableIdx++;
            }

            thisTableIdx++;
        }

        // at the end of this, `thisLvlTables` and `otherLvlTables` should be filled.
        // first add `thisLvlTables` to `entries`, and store all seen Entry. Then add all other entries in `otherLvlTables` that have not yet been seen.
        // within `thisLvlTables`, there are no overlapping keys.
        std::vector<Entry> entries; // `entries`
        std::unordered_set<Entry> seen;

        for (const auto &thisTable : thisLvlTables)
        {
            for (const auto &entry : thisTable->getEntries())
            {
                entries.push_back(entry);
                seen.insert(entry);
            }
        }

        for (const auto &otherTable : otherLvlTables)
        {
            for (const auto &entry : otherTable->getEntries())
            {
                if (!seen.count(entry))
                {
                    entries.push_back(entry);
                }
            }
        }

        thisTableIdx++;

        // delete merged tables, and insert new table into `otherLevel`
        this->_deleteTables(thisLvlTables);
        otherImpl._deleteTables(otherLvlTables);
        otherImpl.createTable(std::move(entries));

        thisLvlTables.clear();
        otherLvlTables.clear();
    }

    return Status::OK();
};

// HELPER FUNCTIONS
// TODO: implement
Status LevelManagerImpl::_mergeOverlappingTables()
{
    TINYKV_LOG("LevelManagerImpl::_mergeOverlappingTables()");

    if (m_ssTables.size() == 0)
        return Status::OK();
    // ASSUME: tables are alr in sorted order (by file number)
    // group tables with overlapping key ranges tgt
    // then for each of these groups

    // sort by startKey
    std::vector<const SSTable *> tables; // stores pointer to this level's tables, sorted based on start keys
    tables.reserve(this->m_ssTables.size());

    for (auto &ssTable : this->m_ssTables)
        tables.push_back(ssTable.get());

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

    auto it = m_ssTables.begin();
    while (it != m_ssTables.end())
    {
        if (victims.count(it->get()))
        {
            m_ssTables.erase(it); // no need to increment `it` as `it` now points to the next element.
        }
        else
        {
            it++;
        }
    }

    return Status::OK();
};
