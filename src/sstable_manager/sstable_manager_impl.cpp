#include "sstable_manager_impl.h"
#include "../level_manager/level_manager_impl.h"
#include "../sstable_file_manager/sstable_file_manager_impl.h"
#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <regex>

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
std::optional<std::vector<Entry>> merge(std::vector<const Entry*> newEntries, std::vector<const Entry*> oldEntries) {
    return std::nullopt;
};

// given a bunch of file managers, groups them based on overlaps
std::vector<std::vector<const SSTableFileManager*>> groupL0Overlaps(std::vector<const SSTableFileManager*> fileManagers) {
    std::vector<std::vector<const SSTableFileManager*>> res;
    return res;
};

std::optional<std::vector<Entry>> mergeL0Entries(std::vector<const SSTableFileManager*>) {
    return std::nullopt;
}

// find files from level N that overlap with `start` and `end`
std::optional<std::vector<const SSTableFileManager*>> SSTableManagerImpl::_findOverlaps(int level, std::string start, std::string end) {
    if (m_levelManagers.size() < level + 1) {
        return std::nullopt;
    }

    auto &levelManager = m_levelManagers[level];
    // return levelManager->
}

std::optional<Error> SSTableManagerImpl::_compactLevel0() {
    std::cout << "[SSTableManagerImpl._compactLevel0]" << "\n";

    // create l1 if it doens't exist
    if (m_levelManagers.size() == 1) {
        std::unique_ptr<LevelManagerImpl> levelManager = std::make_unique<LevelManagerImpl>(1, BASE_PATH + "/level-1");
        if (const auto &err = levelManager->init()) {
            std::cerr << "[SSTableManagerImpl._compactLevel0()]";
            return err;
        }
        m_levelManagers.push_back(std::move(levelManager));
    }

    auto [begin, end] = m_levelManagers[0]->getFiles();

    std::vector<const SSTableFileManager*> level0Files;

    for (auto it = begin; it != end; ++it) {
        level0Files.push_back(it->get());
    }

    std::vector<std::vector<const SSTableFileManager*>> groupedLevel0Files = groupL0Overlaps(level0Files);

    for (const auto& group : groupedLevel0Files) {
        // std::optional<std::vector<Entry>> entries = mergeL0Entries(group);

        // std::vector<const Entry*> entryPtrs;
        // for (const auto &entry : entries.value()) {
        //     entryPtrs.push_back(&entry);
        // }

        // std::string startKey = entries.value()[0].key;
        // std::string endKey = entries.value().back().key;
        // std::optional<std::vector<const SSTableFileManager*>> overlappingFiles = _findOverlaps(1, startKey, endKey);
        
        // // if there are overlapping files in L1, merge with them
        // if (overlappingFiles) {
        //     std::vector<const Entry*> oldEntries;
        //     for (auto &file : overlappingFiles.value())  {
        //         std::optional<std::vector<Entry>> entries = file->getEntries();
        //         for (const auto &entry : entries.value()) {
        //             oldEntries.push_back(&entry);
        //         }
        //     }

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
        // }

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
        if (entry) {
            std::cout << "[SSTableManager.get()] FOUND" << "\n";
            return entry;
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