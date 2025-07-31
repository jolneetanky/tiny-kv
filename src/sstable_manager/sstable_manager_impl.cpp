#include "sstable_manager_impl.h"
#include "../level_manager/level_manager_impl.h"
#include "../sstable_file_manager/sstable_file_manager_impl.h"
#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <regex>

std::vector<SSTableFileManager*> SSTableManagerImpl::getFilesFromLevel(int level) {
    std::vector<SSTableFileManager*> files;
    for (auto& uptr : m_ssTableFileManagers) {
        files.push_back(uptr.get());
    }
    return files;
}

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
        
    // m_ssTableFileManagers.push_back(std::make_unique<SSTableFileManagerImpl>(LEVEL_0_DIRECTORY));

    // Get a reference to the newly added manager
    // SSTableFileManager* newManager = m_ssTableFileManagers.back().get();
    
    // std::optional<Error> errOpt { newManager->write(entries) };
    // if (errOpt) {
    //     std::cerr << "[SSTableManagerImpl.write()] Failed to write to SSTable";
    //     return errOpt;
    // }

    return std::nullopt;

};

std::optional<Entry> SSTableManagerImpl::get(const std::string& key) {
    std::cout << "[SSTableManagerImpl.get()]" << "\n";

    // LOCK DIRECTORY ()
    // NOTE: this is a copy of pointers, modifying it in-place wont change the order of ptrs in ssTablefileManagers
    std::vector<SSTableFileManager*> level0Files { getFilesFromLevel(0)}; // makes a copy

    std::sort(level0Files.begin(), level0Files.end(),
        [](const SSTableFileManager* a, const SSTableFileManager* b) {
            auto ta = a->getTimestamp();
            auto tb = b->getTimestamp();

            // If both have timestamp, compare their values
            // Newer timestamps (with larger values) come first
            if (ta && tb) {
                return ta.value() > tb.value();
            }

            // If only a has timestamp, it is older 
            if (ta && !tb) return true;

            // If only b has timestamp, it should come after a
            if (!ta && tb) return false;

            // If neither has timestamp, consider equal
            return false;
        }
    );

    for (const auto& fileManager : level0Files) {
        // now search in order
        // if key not found, move on to next file
        std::optional<Entry> entryOpt { fileManager->get(key) };
        if (entryOpt && !entryOpt->tombstone) {
            std::cout << "[SSTableManager.get()] FOUND" << "\n";
            return entryOpt;
        } 

        if (entryOpt && entryOpt->tombstone) {
            // key has been deleted
            break;
        }
    }

    std::cout << "[SSTableManager.get()] key does nto exist on disk" << "\n";

    return std::nullopt;
}

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