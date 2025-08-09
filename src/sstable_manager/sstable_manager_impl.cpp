#include "sstable_manager_impl.h"
#include "../level_manager/level_manager_impl.h"
#include "../sstable_file_manager/sstable_file_manager_impl.h"
#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <regex>

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
    // go 2-by-2, level by level

    // CASE #1: LEVEL 0 COMPACTION
    // 
    
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
        m_levelManagers.push_back(std::move(level));
    }

    for (const auto &level : m_levelManagers) {
       std::cout << "HI" << "\n";
    }

    return std::nullopt;
}