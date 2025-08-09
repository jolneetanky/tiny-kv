#include "sstable_manager_impl.h"
#include "../level_manager/level_manager_impl.h"
#include "../sstable_file_manager/sstable_file_manager_impl.h"
#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <unordered_set>
#include "../types/timestamp.h"

// merge files WITHIN level 0, into SSTables with DISTINCT, NON-OVERLAPPING ranges
// std::optional<Error> SSTableManagerImpl::_mergeLevel0() {
//     // levelManagers[0]->mergeLevel0()
// };

std::optional<Error> SSTableManagerImpl::_kWayMerge() {

};

// merges `new` into `old`
// output needs to be sorted
// overwrites current keys with old keys
// the ndeletes tombstoned keys

// KEY ASSUMPTION: the files are given in the order whcih we'd like to merge
// so from leftmost to rightmost is the correct order.
std::optional<std::vector<Entry>> _merge(std::vector<const Entry*> entries) {
    std::cout << "[SSTableManagerImpl._merge()]" << "\n";

    using HeapNode = const Entry*; // Entry and index of manager
    // minheap by key
    // if 2 nodes have the same key, we want the newer one (with larger timestamp) on top
    auto compare = [](const HeapNode &a, const HeapNode &b) {
        return a->key > b->key; // minheap wrt key
    };

    // direct initialization
    std::priority_queue<HeapNode, std::vector<HeapNode>, decltype(compare)>  pq(compare);
    std::unordered_set<std::string> seenKeys;

    for (const auto &entry : entries) {
        // only add to PQ if not yet seen.
        std::string key = entry->key;
        if (seenKeys.find(key) == seenKeys.end()) {
            pq.emplace(entry);
        }
    }

    // for (size_t i = 0; i < fileManagers.size(); ++i) {
    //     auto entries = fileManagers[i]->getEntries();
    //     auto timestamp = fileManagers[i]->getTimestamp();
    //     if (!entries || !timestamp) {
    //         return std::nullopt;
    //     }

    //     for (const auto &entry : entries.value()) {
    //         // add to PQ
    //         pq.emplace(entry, timestamp.value());
    //     }
    // }

    // 3. since we know our PQ is sorted first by key then by timestamp (for duplicates),
    // Pop from PQ and add to `res`. While the next top nodes still have same key, pop them but don't add to res. 
    std::vector<Entry> merged;
    while (!pq.empty()) {
        std::string key = pq.top()->key;
        merged.push_back(*pq.top());
        pq.pop();
        
        // pop duplicates
        while (!pq.empty() && pq.top()->key == key) {
            pq.pop();
        }
    }

    // 4. remove tombstones
    // merged.erase(
    //     std::remove_if(merged.begin(), merged.end(),
    //         [](const Entry &entry) {
    //             return entry.tombstone;
    //         }),
    //     merged.end()
    // );

    return merged;
};

std::optional<std::vector<Entry>> _merge(std::vector<const Entry*> newEntries, std::vector<const Entry*> oldEntries) {
    std::cout << "[SSTableManagerImpl._merge()]" << "\n";
    return std::nullopt;
};

// given a bunch of file managers, groups them based on overlaps
// sort by start key
std::vector<std::vector<const SSTableFileManager*>> SSTableManagerImpl::groupL0Overlaps(std::vector<const SSTableFileManager*> fileManagers) const {
    std::cout << "[SSTableManagerImpl.groupL0Overlaps()]" << "\n";
    std::vector<std::vector<const SSTableFileManager*>> res;

    if (fileManagers.size() == 0) {
        return res;
    }

    // sort by start key
    std::sort(fileManagers.begin(), fileManagers.end(),
        [](const SSTableFileManager* a, const SSTableFileManager* b) {
            return a->getStartKey().value() < b->getStartKey().value();
    });

    // merge into `res
    res.push_back({fileManagers[0]});
    std::string curStart = fileManagers[0]->getStartKey().value(); // the current interval
    std::string curEnd = fileManagers[0]->getEndKey().value();
    
    for (size_t i = 1; i < fileManagers.size(); ++i) {
        // check if this guy belongs to the previous interval
        // in the same interval
        const SSTableFileManager* &fm = fileManagers[i];

        if (fm->getStartKey().value() <= curEnd) {
            std::string endKey = fm->getEndKey().value();
            std::cout << "HEY " << endKey << "\n";
            if (endKey > curEnd) {
                curEnd = endKey;
            }

            auto &prevGroup = res.back();
            prevGroup.push_back(fm);
    
            continue;
        }

        // if not in the same interval, update `curStart` and `curEnd`
        res.push_back({fm});
        curStart = fm->getStartKey().value();
        curEnd = fm->getEndKey().value();
    }

    return res;
};

// merges L entries from oldest to newest
// newest entries overwrite older ones
// tombstoned keys deleted
// ASSUMPTION: earlier SSTableFile write time => all entries in this file should overwrite.
// but this assumption ddoesn't hold across levels. 

// TODO: refactor to be more efficient!
// can ask chatgpt how to do streaming merge like RocksDB.
// heap is expensive esp if we copy everything to heap
std::optional<std::vector<Entry>> _mergeL0Entries(std::vector<const SSTableFileManager*> fileManagers) {
    std::cout << "SSTableManagerImpl.mergeL0Entries()]" << "\n";
    if (fileManagers.empty()) {
        return std::nullopt;
    }

    // 1. sort the SSTableFileManagers by timestamp of each SSTableFile, from newest to oldest

    // descending
    // sort newest -> oldest
    // ie. largest timestamp -> smallest timestamp
    std::sort(fileManagers.begin(), fileManagers.end(),
        [](const SSTableFileManager* a, const SSTableFileManager* b) {
            return a->getTimestamp().value_or(0) > b->getTimestamp().value_or(0);
    });

    // 2. iterate through the sorted file managers. For each entry in a file manager, first check in hashmap keys() if it has already been added to the PQ. If it has, skip it as it has already been overwritten by a newer entry.
    // std::unordered_set<std::string> seenKeys;

    using HeapNode = std::pair<Entry, TimestampType>; // Entry and index of manager
    // minheap by key
    // if 2 nodes have the same key, we want the newer one (with larger timestamp) on top
    auto compare = [](const HeapNode &a, const HeapNode &b) {
        if (a.first.key != b.first.key) {
            return a.first.key > b.first.key;
        }

        // if same key, sort by timestamp
        // larger timestamp first
        return a.second < b.second; 
    };

    // direct initialization
    std::priority_queue<HeapNode, std::vector<HeapNode>, decltype(compare)>  pq(compare);

    for (size_t i = 0; i < fileManagers.size(); ++i) {
        auto entries = fileManagers[i]->getEntries();
        auto timestamp = fileManagers[i]->getTimestamp();
        if (!entries || !timestamp) {
            return std::nullopt;
        }

        for (const auto &entry : entries.value()) {
            // add to PQ
            pq.emplace(entry, timestamp.value());
        }
    }

    // 3. since we know our PQ is sorted first by key then by timestamp (for duplicates),
    // Pop from PQ and add to `res`. While the next top nodes still have same key, pop them but don't add to res. 
    std::vector<Entry> merged;
    while (!pq.empty()) {
        std::string key = pq.top().first.key;
        merged.push_back(pq.top().first);
        pq.pop();
        
        // pop duplicates
        while (!pq.empty() && pq.top().first.key == key) {
            pq.pop();
        }
    }

    // 4. remove tombstones
    // merged.erase(
    //     std::remove_if(merged.begin(), merged.end(),
    //         [](const Entry &entry) {
    //             return entry.tombstone;
    //         }),
    //     merged.end()
    // );

    return merged;
}

// find files from level N that overlap with `start` and `end`
std::optional<std::vector<const SSTableFileManager*>> SSTableManagerImpl::_getOverlaps(int level, std::string start, std::string end) const {
    std::cout << "[SSTableManagerImpl._getOverlaps()]" << "\n";

    if (m_levelManagers.size() < level + 1) {
        return std::nullopt;
    }

    // iterate through the files and get overlapping files
    auto &levelManager = m_levelManagers[level];
    std::vector<const SSTableFileManager*> res;
    auto [itBegin, itEnd] = levelManager->getFiles();

    for (auto it = itBegin; it != itEnd; ++it) {
        const SSTableFileManager* fm = it->get();

        auto startKey = fm->getStartKey();
        auto endKey = fm->getEndKey();

        if (startKey <= end && endKey >= start) {
            res.push_back(fm);
        }
    }

    return res;

    // return levelManager->
}

std::optional<Error> SSTableManagerImpl::_compactLevel0() {
    std::cout << "[SSTableManagerImpl._compactLevel0]" << "\n";

    // create l1 if it doens't exist
    if (m_levelManagers.size() == 1) {
        std::unique_ptr<LevelManagerImpl> levelManager = std::make_unique<LevelManagerImpl>(1, BASE_PATH + "/level-1");
        if (const auto &err = levelManager->init()) {
            std::cerr << "[SSTableManagerImpl._compactLevel0()] Failed to init level 1";
            return err;
        }
        m_levelManagers.push_back(std::move(levelManager));
    }

    // 2. get level 0 file managers
    auto [begin, end] = m_levelManagers[0]->getFiles();
    std::vector<const SSTableFileManager*> level0Files;
    for (auto it = begin; it != end; ++it) {
        level0Files.push_back(it->get());
    }

    // 3. group overlapping files tgt
    std::vector<std::vector<const SSTableFileManager*>> groupedLevel0Files = groupL0Overlaps(level0Files);

    // 4. for each group of files, merge the entries
    for (const auto& group : groupedLevel0Files) {
        std::cout << "GROUP" << "\n";

        // merge the files of each groups into `entries`
        std::cout << "[SSTableManagerImpl._compactLevel0] Merging within level..." << "\n";
        std::optional<std::vector<Entry>> entries = _mergeL0Entries(group);

        if (!entries) {
            std::string errMsg = "Failed to merge L0 entries";
            std::cout << "[SSTableFileManager._compactLevel0] " << errMsg << "\n";
            return Error{ errMsg };
        }
        
        for (const auto &entry : entries.value()) {
            std::cout << "KEY: " << entry.key << ", VAL: " << entry.val << "\n";
        }

        // 5. find overlapping files from L1

        std::string startKey = entries.value()[0].key;
        std::string endKey = entries.value().back().key;
        std::optional<std::vector<const SSTableFileManager*>> overlappingFiles = _getOverlaps(1, startKey, endKey);

        if (!overlappingFiles) {
            return Error{ "Failed to get overlapping files from L1" };
        }
        
        std::vector<const Entry*> entryPtrs;
        for (const auto &entry : entries.value()) {
            entryPtrs.push_back(&entry);
        }

        // // if there are overlapping files in L1, merge with them
        // if no overlapping, just write to level 1
        if (overlappingFiles.value().size() == 0) {
            std::cout << "No overlaps detected" << "\n";
            m_levelManagers[1]->writeFile(entryPtrs);
            m_levelManagers[0]->deleteFiles(group);
            continue;
        }

        // for each file, dump into a vector `oldEntries`
        std::cout << "OVERLAPS DETECTED" << "\n";
        for (auto &file : overlappingFiles.value())  {
            std::optional<std::vector<Entry>> entries = file->getEntries();
            for (const auto &entry : entries.value()) {
                entryPtrs.push_back(&entry);
            }
        }

        std::cout << "[SSTableManagerImpl._compactLevel0] Merging L0 and L1" << "\n";
        std::optional<std::vector<Entry>> mergedEntries = _merge(entryPtrs);

        if (!mergedEntries) {
            return Error{ "Failed to merge entries of L0 and L1" };
        }

        std::vector<const Entry*> mergedEntriesPtr;

        for (const auto &mergedEntry : mergedEntries.value()) {
            std::cout << "KEY: " << mergedEntry.key << ", VAL: " << mergedEntry.val << "\n";
            mergedEntriesPtr.push_back(&mergedEntry);
        }

        m_levelManagers[1]->writeFile(mergedEntriesPtr);

        // delete merged files
        m_levelManagers[0]->deleteFiles(group);
        m_levelManagers[1]->deleteFiles(overlappingFiles.value());

        //     std::optional<std::vector<Entry>> mergedEntries = merge(entries.value(), oldEntries);
        //     std::vector<const Entry*> mergedEntriesPtr;

        //     for (const auto &entry : mergedEntries.value()) {
        //         mergedEntriesPtr.push_back(&entry);
        //     }
            
        //     // write merged entries to a file, into level 1
        //     m_levelManagers[1]->writeFile(mergedEntriesPtr);

        //     // delete overlapping files in level 1
        //     m_levelManagers[1]->deleteFiles(overlappingFiles.value());

        //     continue;

        // // nothing in L1 to merge with; simply wirte the entries as they are to L1
        // m_levelManagers[1]->writeFile(entryPtrs);

    }

    // group overlapping SSTableFileManagers tgt
    // start idx, end idx

    // for each of these groups `std::vector<const SSTableFileManager*>`
    // merge into std::vector<Entry*> -> these are the entries we want to merge
    // then look for files in L1 that overlap
    // 

    // get overlapping files from l1
    // write everything into a 

    return std::nullopt;
    // overlappingFiles = getOverlappingFiles(m_levelManagers[1]);
    // mergedEntries = merge(level0Files, overlappingFiles)
    // m_levelManagers[1]->write(mergedEntries);
    // m_levelManagers[0]->deleteFiles(level0Files)
    // m_levelManagers[1]->deleteFiles(overlappingFiles)

    // levelManagers[0]->merge()
    // for `file` in `levelManagers[0]->files`:
    // entries = _kWayMerge(file, levelManagers[1]->getOverlappingFiles(rangeStart, rangeEnd))
    // levelManagers[0]->deleteFiles(levelManagers[0]->files)
    // levelManagers[1]->deletefiles(overlappingFiles)

    // merge level 0 files
    // do we merge all? it'll be like merge intervals lol

    // after merging => merge all level 0 files to level 1, skipping the ones where range doesn't overlap
    // this step onward is basically _compactLevelN(int n).
};

std::optional<Error> SSTableManagerImpl::_compactLevelN(int n) {
    // get the oldest file from level `n` (with the oldest timestamp)
    // merge the file from level `n` with overlapping files in level `n+1`
};

// write all entries into a file (serialize each entry)
std::optional<Error> SSTableManagerImpl::write(std::vector<const Entry*> entries, int level) {
    std::cout << "[SSTableManagerImpl.write()]" << std::endl;

    if (m_levelManagers.size() == 0) {
        initLevels();
    }

    auto& level0Manager { m_levelManagers[0] };

    // we can just write as it is because the memtable entries are sorted.
    std::optional<Error> errOpt { level0Manager->writeFile(entries) };
    if (errOpt) {
        std::cerr << "[SSTableManagerImpl.write()] Failed to write SSTable" << std::endl;
        return errOpt;
    }
        
    return std::nullopt;

};

std::optional<Entry> SSTableManagerImpl::get(const std::string& key) const {
    std::cout << "[SSTableManagerImpl.get()]" << "\n";

    // search all levels
    for (const auto &levelManager : m_levelManagers) {
        std::optional<Entry> entry { levelManager->searchKey(key) };
        if (entry && !entry->tombstone) {
            std::cout << "[SSTableManager.get()] FOUND" << "\n";
            return entry;
        }
        if (entry && entry->tombstone) {
            break;
        }
    }

    std::cout << "[SSTableManager.get()] key does not exist on disk" << "\n";
    return std::nullopt;
}

std::optional<Error> SSTableManagerImpl::compact() {
    _compactLevel0();

    return std::nullopt;
    // go 2-by-2, level by level

    // CASE #1: LEVEL 0 COMPACTION
    // merge multiple files 
    
    // L0: a-g, c-d
    // L1: a-c, e-h, l-p

    // one-by-one:
    // L0: c-d
    // L1: a-h, l-p

    // L0: empty
    // L1: a-h, l-p 

    // merge overlapping in L0:
    // L0: a-g

    // n + m
    // n + (n + m) = 2n + m
    // ...
    // kn + m

    // total = (n + m) + (2n + m) + ... + (kn + m)
    // = n(1 + 2 + ... + k) + km
    // = O(k^2n + km)

    
    // CASE #2: LEVEL N COMPACTION (N > 0)
    // compact level N (N > 0) => pick the oldest SSTable file from level N
    // for that file, search for files in N+1 with overlapping key ranges.
    // merge these files
    // delete merged files from level N and level N+1
    // write the new merged file to level N+1 
};

// for every directory in "./sstables/"
// in ascending order of file name (because name of each level folder will be like l`level-0, level-1`, etc)
// i need to create a new FileManager
// and push it into m_levels, of type `std::vector<std::unique_ptr<LevelManager>>`
std::optional<Error> SSTableManagerImpl::initLevels() {
    std::cout << "[SSTableManagerImpl::initLevels()]" << "\n";

    const std::string &basePath = BASE_PATH;
    const std::string &level0Path = basePath + "/level-0";
    std::vector<std::pair<int, std::filesystem::path>> levelDirs;

    // create directory ./sstables/level-0 if it does not alr exist
    try {
        if (std::filesystem::create_directories(level0Path)) {
            std::cout << "[SSTableManagerImpl::initLevels()] Successfully created directory " << level0Path << "\n";
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "[SSTableManagerImpl::initLevels()] Error creating directories: " << e.what() << "\n";
        return Error{ "Failed to create directory" + level0Path };
    }

    // Step 1: iterate through the base directory
    // add folders that match the pattern `level-<x>` into `levelDirs`
    for (const auto& entry : std::filesystem::directory_iterator(basePath)) {
        if (!entry.is_directory()) continue;

        std::string folderName = entry.path().filename().string();

        std::smatch match;
        std::regex pattern(R"(level-(\d+))");

        if (std::regex_match(folderName, match, pattern)) {
            int levelNum = std::stoi(match[1].str());
            levelDirs.emplace_back(levelNum, entry.path());
        }
    }

    // Step 2: sort by level number
    std::sort(levelDirs.begin(), levelDirs.end());

    // Step 3: create LevelManager and push to m_levels
    // Place in index corresponding to level number
    for (const auto &[levelNum, path] : levelDirs) {
        std::cout << "[SSTableManagerImpl::initLevels()]" << path.string() << "\n";
        auto level = std::make_unique<LevelManagerImpl>(levelNum, path.string());
        // initialize level with files
        if (auto err = level->init()) {
            return err;
        }

        m_levelManagers.push_back(std::move(level)); // destroys the local `level` after copying the value
    }

    return std::nullopt;
}