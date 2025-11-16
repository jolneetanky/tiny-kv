#include "sstable_manager_impl.h"
#include "core/level_manager/level_manager_impl.h"
#include "core/sstable_file_manager/sstable_file_manager_impl.h"
#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <unordered_set>
#include <limits>
#include "types/timestamp.h"

SSTableManagerImpl::SSTableManagerImpl(SystemContext &systemContext) : m_systemContext{systemContext} {};

// KEY ASSUMPTION: the files are given in the order whcih we'd like to merge
// so from leftmost to rightmost is the correct order.
std::optional<std::vector<Entry>> SSTableManagerImpl::_mergeEntries(std::vector<const Entry *> entries) const
{
    std::cout << "[SSTableManagerImpl._mergeEntries()]" << "\n";

    using HeapNode = const Entry *; // Entry and index of manager
    // minheap by key
    // if 2 nodes have the same key, we want the newer one (with larger timestamp) on top
    auto compare = [](const HeapNode &a, const HeapNode &b)
    {
        return a->key > b->key; // minheap wrt key
    };

    // direct initialization
    std::priority_queue<HeapNode, std::vector<HeapNode>, decltype(compare)> pq(compare);
    std::unordered_set<std::string> seenKeys;

    // no duplicates added to PQ
    for (const auto &entry : entries)
    {
        // only add to PQ if not yet seen.
        std::string key = entry->key;
        if (seenKeys.find(key) == seenKeys.end())
        {
            pq.emplace(entry);
        }
        seenKeys.insert(key);
    }

    // we know all keys in our PQ are unique
    std::vector<Entry> merged;
    while (!pq.empty())
    {
        std::string key = pq.top()->key;
        merged.push_back(*pq.top());
        pq.pop();
    }

    return merged;
};

// given a bunch of file managers, groups them based on overlaps
// sort by start key
std::vector<std::vector<SSTableFileManager *>> SSTableManagerImpl::groupL0Overlaps(std::vector<SSTableFileManager *> fileManagers) const
{
    std::cout << "[SSTableManagerImpl.groupL0Overlaps()]" << "\n";
    std::vector<std::vector<SSTableFileManager *>> res;

    // filter out those without start keys or end keys
    fileManagers.erase(
        std::remove_if(fileManagers.begin(), fileManagers.end(),
                       [](SSTableFileManager *fm)
                       {
                           return !fm->getStartKey().has_value() || !fm->getEndKey().has_value();
                       }),
        fileManagers.end());

    if (fileManagers.size() == 0)
    {
        return res;
    }

    // --- ERROR IN HERE ---
    // sort by start key
    // if no start key, ignore. Or make it have an arbitary start key value cause it really doesn't matter
    std::sort(fileManagers.begin(), fileManagers.end(),
              [](SSTableFileManager *a, SSTableFileManager *b)
              {
                  return a->getStartKey().value() < b->getStartKey().value();
              });

    // merge into `res
    res.push_back({fileManagers[0]});
    std::string curStart = fileManagers[0]->getStartKey().value();
    std::string curEnd = fileManagers[0]->getEndKey().value();

    for (size_t i = 1; i < fileManagers.size(); ++i)
    {
        // check if this guy belongs to the previous interval
        // in the same interval
        SSTableFileManager *&fm = fileManagers[i];

        if (fm->getStartKey().value() <= curEnd)
        {
            std::string endKey = fm->getEndKey().value();
            if (endKey > curEnd)
            {
                curEnd = endKey;
            }

            auto &prevGroup = res.back();
            prevGroup.push_back(fm);

            continue;
        }

        res.push_back({fm});
        curStart = fm->getStartKey().value();
        curEnd = fm->getEndKey().value();
    }

    return res;
};

// find files from level `level` that overlap with `start` and `end`
std::optional<std::vector<SSTableFileManager *>> SSTableManagerImpl::_getOverlappingFiles(int level, std::string start, std::string end) const
{
    std::cout << "[SSTableManagerImpl._getOverlappingFiles()]" << "\n";

    if (m_levelManagers.size() < level + 1)
    {
        return std::nullopt;
    }

    // iterate through the files and get overlapping files
    auto &levelManager = m_levelManagers[level];
    std::vector<SSTableFileManager *> res;
    auto [itBegin, itEnd] = levelManager->getFiles();

    for (auto it = itBegin; it != itEnd; ++it)
    {
        SSTableFileManager *fm = it->get();

        auto startKey = fm->getStartKey();
        auto endKey = fm->getEndKey();

        if (startKey <= end && endKey >= start)
        {
            res.push_back(fm);
        }
    }

    return res;
}

std::optional<Error> SSTableManagerImpl::_compactLevel0()
{
    std::cout << "[SSTableManagerImpl._compactLevel0]" << "\n";

    // 1. create l1 if it doesn't exist
    if (m_levelManagers.size() == 1)
    {
        std::unique_ptr<LevelManagerImpl> levelManager = std::make_unique<LevelManagerImpl>(1, BASE_PATH + "/level-1", m_systemContext);
        if (const auto &err = levelManager->init())
        {
            std::cerr << "[SSTableManagerImpl._compactLevel0()] Failed to init level 1";
            return err;
        }
        m_levelManagers.push_back(std::move(levelManager));
    }

    // 2. get level 0 file managers
    auto [begin, end] = m_levelManagers[0]->getFiles();
    std::vector<SSTableFileManager *> level0Files;
    for (auto it = begin; it != end; ++it)
    {
        level0Files.push_back(it->get());
    }

    // 3. group overlapping files tgt
    std::vector<std::vector<SSTableFileManager *>> groupedLevel0Files = groupL0Overlaps(level0Files);

    // 4. for each group of files, merge the entries
    for (auto &fileManagers : groupedLevel0Files)
    {
        std::cout << "GROUP" << "\n";

        std::cout << "[SSTableManagerImpl._compactLevel0] Merging within level..." << "\n";

        // sort `group` by timestamp, from newest to oldest
        std::sort(fileManagers.begin(), fileManagers.end(),
                  [](SSTableFileManager *a, SSTableFileManager *b)
                  {
                      return a->getTimestamp().value_or(0) > b->getTimestamp().value_or(0);
                  });

        // store all entries into `entriesToMerge`.
        // then push entries into a vector
        std::vector<Entry> entriesToMerge;
        std::vector<const Entry *> entriesToMergePtrs;

        for (auto &fm : fileManagers)
        {
            std::vector<Entry> emptyEntries{};
            const auto &entries = fm->getEntries().value_or(emptyEntries);

            for (const auto &entry : entries)
            {
                entriesToMerge.push_back(entry);
            }
        }

        for (const auto &entry : entriesToMerge)
        {
            entriesToMergePtrs.push_back(&entry);
        }

        // merge the entries to handle newer overwrites
        // `entries` stores 1) the particular group of entries from L0, 2) overlapping entries from L1.
        std::optional<std::vector<Entry>> entries = _mergeEntries(entriesToMergePtrs);

        if (!entries)
        {
            std::string errMsg = "Failed to merge L0 entries";
            std::cout << "[SSTableFileManager._compactLevel0] " << errMsg << "\n";
            return Error{errMsg};
        }

        for (const auto &entry : entries.value())
        {
            std::cout << "KEY: " << entry.key << ", VAL: " << entry.val << "\n";
        }

        // 5. find overlapping files from L1, store in `overlappingFiles`
        std::string startKey = entries.value()[0].key;
        std::string endKey = entries.value().back().key;
        std::optional<std::vector<SSTableFileManager *>> overlappingFiles = _getOverlappingFiles(1, startKey, endKey);

        if (!overlappingFiles)
        {
            return Error{"Failed to get overlapping files from L1"};
        }

        std::vector<const Entry *> entryPtrs; // stores pointers of entries from L0 + entries from L1 that we're gonna merge
        for (const auto &entry : entries.value())
        {
            entryPtrs.push_back(&entry);
        }

        std::vector<const SSTableFileManager *> constFileManagers;
        for (const auto &fm : fileManagers)
        {
            constFileManagers.push_back(fm);
        }

        // if no overlapping files in L1, just write the current (merged) entries from this group in L0
        if (overlappingFiles.value().size() == 0)
        {
            std::cout << "No overlaps detected" << "\n";
            m_levelManagers[1]->writeFile(entryPtrs);
            m_levelManagers[0]->deleteFiles(constFileManagers);
            continue;
        }

        // if there are overlapping files in L1, get their entries and push into vector `entryPtrs`.
        std::cout << "OVERLAPS DETECTED" << "\n";
        for (auto &file : overlappingFiles.value())
        {
            std::optional<std::vector<Entry>> entries = file->getEntries();
            for (const auto &entry : entries.value())
            {
                entryPtrs.push_back(&entry);
            }
        }

        // 6. merge the entries stored in `entryPtrs`
        std::cout << "[SSTableManagerImpl._compactLevel0] Merging L0 and L1" << "\n";
        std::optional<std::vector<Entry>> mergedEntries = _mergeEntries(entryPtrs);

        if (!mergedEntries)
        {
            return Error{"Failed to merge entries of L0 and L1"};
        }

        std::vector<const Entry *> mergedEntriesPtr;

        for (const auto &mergedEntry : mergedEntries.value())
        {
            std::cout << "KEY: " << mergedEntry.key << ", VAL: " << mergedEntry.val << "\n";
            mergedEntriesPtr.push_back(&mergedEntry);
        }

        // 7. write the merged entries to level 1
        m_levelManagers[1]->writeFile(mergedEntriesPtr);

        // 8. delete merged files from level 0
        m_levelManagers[0]->deleteFiles(constFileManagers);

        // 9. delete merged files from level 1
        std::vector<const SSTableFileManager *> constOverlappingFiles;
        for (const auto &fm : overlappingFiles.value())
        {
            constOverlappingFiles.push_back(fm);
        }
        m_levelManagers[1]->deleteFiles(constOverlappingFiles);
    }

    return std::nullopt;
};

std::optional<Error> SSTableManagerImpl::_compactLevelN(int n)
{
    std::cout << "[SSTableManagerImpl.compactLevelN()]" << "\n";
    // get the oldest file in level N (with the oldest timestamp -> this is the time at which it was compacted and written to level n)
    if (n == 0)
    {
        return Error{"Cannot use SSTableManagerImpl::_compactLevelN() if n == 0"};
    }

    if (n == MAX_LEVEL)
    {
        return Error{"Max level of SSTable reached. Cannot compact further."};
    }

    if (m_levelManagers.size() < n + 1)
    {
        return Error{"Level doesn't exist."};
    }

    // 1. Create level N + 1 if it doesn't exist
    if (m_levelManagers.size() < n + 2)
    {
        std::unique_ptr<LevelManagerImpl> levelManager = std::make_unique<LevelManagerImpl>(n + 1, BASE_PATH + "/level-" + std::to_string(n + 1), m_systemContext);
        if (const auto &err = levelManager->init())
        {
            std::cerr << "[SSTableManagerImpl._compactLevelN()] Failed to init level " + std::to_string(n + 1) << "\n";
            return err;
        }
        m_levelManagers.push_back(std::move(levelManager));
    }

    // 2. Get the oldest file from level N
    // std::unique_ptr<LevelManager> &lm = m_levelManagers[n];
    TimestampType curMin = std::numeric_limits<TimestampType>::max();
    SSTableFileManager *oldestFile; // level N oldest file

    auto [begin, end] = m_levelManagers[n]->getFiles();

    // check if there are even files on this level
    if (begin == end)
    {
        return std::nullopt;
    }

    for (auto it = begin; it != end; ++it)
    {
        const auto &fm = it->get();
        // find the file with smallest timestamp
        if (fm->getTimestamp().value() < curMin)
        {
            curMin = fm->getTimestamp().value();
            oldestFile = fm;
        }
    }

    const auto &startKey = oldestFile->getStartKey();
    const auto &endKey = oldestFile->getEndKey();

    if (!startKey || !endKey)
    {
        std::cerr << "[SSTableManagerImpl._compactLevelN()] Failed to get start or end key of oldest file" << "\n";
        return Error{"Failed to compact level " + std::to_string(n + 1)};
    }

    std::vector<const Entry *> entryPtrs; // stores the entries to be merged. Merge direction is from left to right.

    // 4. place the level N entries we wanna merge AT THE FRONT OF `entryPtrs`

    std::vector<Entry> emptyEntries{};
    const auto &levelNEntries = oldestFile->getEntries().value_or(emptyEntries);

    for (const auto &entry : levelNEntries)
    {
        entryPtrs.push_back(&entry);
    }

    // 3. get overlapping files from level N + 1, and add their entries into `entryPtrs`
    const auto &overlappingFiles = _getOverlappingFiles(n + 1, startKey.value(), endKey.value());

    if (!overlappingFiles)
    {
        std::cerr << "[SSTableManagerImpl._compactLevelN()] Failed to get overlapping files from level n + 1" << "\n";
        return Error{"Failed to compact level " + std::to_string(n + 1)};
    }

    for (const auto &file : overlappingFiles.value())
    {
        // add entries into entryPtrs
        std::vector<Entry> emptyEntries{};
        const auto &entries = file->getEntries().value_or(emptyEntries);

        for (const auto &entry : entries)
        {
            entryPtrs.push_back(&entry);
        }
    }

    // 4. Merge entries
    std::cout << "[SSTableManagerImpl._compactLevelN] Merging LN and L(N+1)" << "\n";
    std::optional<std::vector<Entry>> mergedEntries = _mergeEntries(entryPtrs);

    if (!mergedEntries)
    {
        return Error{"Failed to merge entries of L0 and L1"};
    }

    // remove tombstones if we are the second last level
    if (n == MAX_LEVEL - 1)
    {
        mergedEntries.value().erase(
            std::remove_if(mergedEntries.value().begin(), mergedEntries.value().end(),
                           [](const Entry &entry)
                           {
                               return entry.tombstone;
                           }),
            mergedEntries.value().end());
    }

    // 5. Write merged entries to a file in level n+1, and delete the merged files
    std::vector<const Entry *> mergedEntriesPtr;

    for (const auto &mergedEntry : mergedEntries.value())
    {
        std::cout << "KEY: " << mergedEntry.key << ", VAL: " << mergedEntry.val << "\n";
        mergedEntriesPtr.push_back(&mergedEntry);
    }

    m_levelManagers[n + 1]->writeFile(mergedEntriesPtr);

    // delete merged files
    m_levelManagers[n]->deleteFiles({oldestFile});
    // create const pointer so i can use for `LevelManager.deleteFiles()`
    std::vector<const SSTableFileManager *> constOverlappingFiles;
    for (const auto &fm : overlappingFiles.value())
    {
        constOverlappingFiles.push_back(fm);
    }
    m_levelManagers[n + 1]->deleteFiles(constOverlappingFiles);

    return std::nullopt;
};

// write all entries into a file (serialize each entry)
std::optional<Error> SSTableManagerImpl::write(std::vector<const Entry *> entries, int level)
{
    std::cout << "[SSTableManagerImpl.write()]" << std::endl;

    if (m_levelManagers.size() == 0)
    {
        initLevels();
    }

    auto &level0Manager{m_levelManagers[0]};

    // we can just write as it is because the memtable entries are sorted.

    std::optional<Error> errOpt{level0Manager->writeFile(entries)};
    if (errOpt)
    {
        std::cerr << "[SSTableManagerImpl.write()] Failed to write SSTable" << std::endl;
        return errOpt;
    }

    return std::nullopt;
};

std::optional<Entry> SSTableManagerImpl::get(const std::string &key) const
{
    std::cout << "[SSTableManagerImpl.get()]" << "\n";

    // search all levels
    for (const auto &levelManager : m_levelManagers)
    {
        std::optional<Entry> entry{levelManager->searchKey(key)};
        if (entry && !entry->tombstone)
        {
            std::cout << "[SSTableManager.get()] FOUND" << "\n";
            return entry;
        }
        if (entry && entry->tombstone)
        {
            break;
        }
    }

    std::cout << "[SSTableManager.get()] key does not exist on disk" << "\n";
    return std::nullopt;
}

// TO TEST:
// 1) _compactLevel0()
// 2) _compactLevel1()
std::optional<Error> SSTableManagerImpl::compact()
{
    _compactLevel0();

    for (int lvl = 1; lvl < MAX_LEVEL; ++lvl)
    {
        _compactLevelN(lvl);
    }

    return std::nullopt;
};

// for every directory in "./sstables/"
// in ascending order of file name (because name of each level folder will be like l`level-0, level-1`, etc)
// i need to create a new FileManager
// and push it into m_levels, of type `std::vector<std::unique_ptr<LevelManager>>`
std::optional<Error> SSTableManagerImpl::initLevels()
{
    std::cout << "[SSTableManagerImpl::initLevels()]" << "\n";

    const std::string &basePath = BASE_PATH;
    const std::string &level0Path = basePath + "/level-0";
    std::vector<std::pair<int, std::filesystem::path>> levelDirs;

    // create directory ./sstables/level-0 if it does not alr exist
    try
    {
        if (std::filesystem::create_directories(level0Path))
        {
            std::cout << "[SSTableManagerImpl::initLevels()] Successfully created directory " << level0Path << "\n";
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "[SSTableManagerImpl::initLevels()] Error creating directories: " << e.what() << "\n";
        return Error{"Failed to create directory" + level0Path};
    }

    // Step 1: iterate through the base directory
    // add folders that match the pattern `level-<x>` into `levelDirs`
    for (const auto &entry : std::filesystem::directory_iterator(basePath))
    {
        if (!entry.is_directory())
            continue;

        std::string folderName = entry.path().filename().string();

        std::smatch match;
        std::regex pattern(R"(level-(\d+))");

        if (std::regex_match(folderName, match, pattern))
        {
            int levelNum = std::stoi(match[1].str());
            levelDirs.emplace_back(levelNum, entry.path());
        }
    }

    // Step 2: sort by level number
    std::sort(levelDirs.begin(), levelDirs.end());

    // Step 3: create LevelManager and push to m_levels
    // Place in index corresponding to level number
    for (const auto &[levelNum, path] : levelDirs)
    {
        std::cout << "[SSTableManagerImpl::initLevels()]" << path.string() << "\n";
        auto level = std::make_unique<LevelManagerImpl>(levelNum, path.string(), m_systemContext);
        // initialize level with files
        if (auto err = level->init())
        {
            return err;
        }

        m_levelManagers.push_back(std::move(level)); // destroys the local `level` after copying the value
    }

    return std::nullopt;
}