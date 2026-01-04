#include "core/disk_manager/disk_manager_impl_new.h"
#include "core/level_manager/level_manager_impl.h"
#include "core/sstable_manager/sstable_manager_impl.h"
#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <unordered_set>
#include <limits>
#include <utility>
#include "types/timestamp.h"
#include "types/status.h"

DiskManagerImpl::DiskManagerImpl(SystemContext &systemContext, std::string basePath)
    : m_basePath{std::move(basePath)}, m_systemContext{systemContext} {};

// write all entries into a file (serialize each entry)
std::optional<Error> DiskManagerImpl::write(const std::vector<const Entry *> &entryPtrs, int level)
{
    std::cout << "[DiskManagerImpl.write()]" << std::endl;

    if (m_levelManagers.size() == 0)
    {
        initLevels();
    }

    auto &level0Manager{m_levelManagers[0]};

    std::vector<Entry> entries;
    entries.reserve(entryPtrs.size());
    for (auto &ptr : entryPtrs)
    {
        entries.emplace_back(*ptr);
    }

    Status status{level0Manager->createTable(std::move(entries))};
    if (!status.ok())
    {
        std::cerr << "[DiskManagerImpl.write()] Failed to write SSTable" << std::endl;
        return std::optional<Error>(status.to_string());
    }

    return std::nullopt;
};

std::optional<Entry> DiskManagerImpl::get(const std::string &key) const
{
    std::cout << "[DiskManagerImpl.get()]" << "\n";

    // search all levels
    for (const auto &levelManager : m_levelManagers)
    {
        std::optional<Entry> entry{levelManager->getKey(key)};
        if (entry && !entry->tombstone)
        {
            std::cout << "[DiskManager.get()] FOUND: " << entry.value() << "\n";
            return entry;
        }
        if (entry && entry->tombstone)
        {
            break;
        }
    }

    std::cout << "[DiskManager.get()] key does not exist on disk" << "\n";
    return std::nullopt;
};

std::optional<Error> DiskManagerImpl::compact()
{
    // merge overlapping entries in L0
    // merge level `i` with level `i+1`, for all 0 <= i <= n-1
    if (m_levelManagers.size() == 0)
        return std::nullopt;

    for (int lvl = 0; lvl < MAX_LEVEL; lvl++)
    {
        m_levelManagers[lvl]->compactInto(*m_levelManagers[lvl + 1]);
        // 1. For every SSTable in this levelManager, pass it to `levelManager[lvl+1]`, and ask it to overwrite its own overlapping file with these entries
        // 2. Even better, do something like `levelManager[lvl].compactInto(levelManager[lvl+1])`
    }

    return std::nullopt;
};

// initializes the level managers based on existing folders on disk.
// Creates all file managers up to MAX_LEVEL for remaining levels that are not yet filled.
// EDGE CASE: if eg. there are 5 levels, but we change MAX_LEVEL to 2 this time round.
// std::optional<Error> DiskManagerImpl::initLevels()
// {
//     std::cout << "[DiskManagerImpl::initLevels()]" << std::endl;

//     const std::string &basePath = m_basePath;
//     const std::string &level0Path = basePath + "/level-0";
//     std::vector<std::pair<int, std::filesystem::path>> levelDirs;

//     // create directory ./sstables/level-0 if it does not alr exist
//     try
//     {
//         if (std::filesystem::create_directories(level0Path))
//         {
//             std::cout << "[DiskManagerImpl::initLevels()] Successfully created directory " << level0Path << std::endl;
//         }
//     }
//     catch (const std::filesystem::filesystem_error &e)
//     {
//         std::cerr << "[DiskManagerImpl::initLevels()] Error creating directories: " << e.what() << "\n";
//         return Error{"Failed to create directory" + level0Path};
//     }

//     // Step 1: iterate through the base directory
//     // add folders that match the pattern `level-<x>` into `levelDirs`
//     for (const auto &entry : std::filesystem::directory_iterator(basePath))
//     {
//         if (!entry.is_directory())
//             continue;

//         std::string folderName = entry.path().filename().string();

//         std::smatch match;
//         std::regex pattern(R"(level-(\d+))");

//         if (std::regex_match(folderName, match, pattern))
//         {
//             int levelNum = std::stoi(match[1].str());
//             levelDirs.emplace_back(levelNum, entry.path());
//         }
//     }

//     // Step 2: sort by level number
//     std::sort(levelDirs.begin(), levelDirs.end());

//     // Step 3: create LevelManager and push to m_levels
//     // Place in index corresponding to level number
//     for (const auto &[levelNum, path] : levelDirs)
//     {
//         auto level = std::make_unique<LevelManagerImpl>(levelNum, path.string(), m_systemContext);
//         Status status = level->initNew();
//         // initialize level with files
//         if (!status.ok())
//         {
//             return Error(status.to_string());
//         }

//         m_levelManagers.push_back(std::move(level)); // destroys the local `level` after copying the value
//     }

//     return std::nullopt;
// };

// initializes the level managers based on existing folders on disk.
// Creates all file managers up to MAX_LEVEL for remaining levels that are not yet filled.
// EDGE CASE: if eg. there are 5 levels, but we change MAX_LEVEL to 2 this time round.
std::optional<Error> DiskManagerImpl::initLevels()
{
    std::cout << "[DiskManagerImpl::initLevels()]" << std::endl;

    const std::string &basePath = m_basePath;

    // Ensure base directory exists
    std::error_code ec;
    std::filesystem::create_directories(basePath, ec);
    if (ec)
    {
        return Error{"Failed to create base directory: " + ec.message()};
    }

    // Map: levelNum -> directory path
    std::unordered_map<int, std::filesystem::path> discoveredLevels;

    // Step 1: scan base directory
    for (const auto &entry : std::filesystem::directory_iterator(basePath))
    {
        if (!entry.is_directory())
            continue;

        const std::string folderName = entry.path().filename().string();
        std::smatch match;

        static const std::regex pattern(R"(level-(\d+))");
        if (!std::regex_match(folderName, match, pattern))
            continue;

        int levelNum = std::stoi(match[1].str());

        // EDGE CASE: ignore levels beyond MAX_LEVEL
        if (levelNum > MAX_LEVEL)
        {
            std::cout << "[DiskManagerImpl::initLevels()] Ignoring level-"
                      << levelNum << " (exceeds MAX_LEVEL)" << std::endl;
            continue;
        }

        discoveredLevels.emplace(levelNum, entry.path());
    }

    // Step 2: create LevelManagers from 0 .. MAX_LEVEL
    m_levelManagers.clear(); // clear JIC we currently have some populated state
    m_levelManagers.reserve(MAX_LEVEL + 1);

    for (int levelNum = 0; levelNum <= MAX_LEVEL; ++levelNum)
    {
        std::string levelPath = basePath + "/level-" + std::to_string(levelNum);

        auto level = std::make_unique<LevelManagerImpl>(
            levelNum,
            levelPath,
            m_systemContext);

        Status status;
        if (discoveredLevels.count(levelNum))
        {
            // Existing directory → load SSTables
            status = level->initNew();
        }
        else
        {
            // New level → create directory here (DiskManager responsibility)
            std::error_code ec;
            std::filesystem::create_directories(levelPath, ec);
            if (ec)
            {
                return Error{
                    "Failed to create directory " + levelPath + ": " + ec.message()};
            }
        }

        if (!status.ok())
        {
            return Error{
                "Failed to initialize level-" +
                std::to_string(levelNum) +
                ": " + status.to_string()};
        }

        m_levelManagers.push_back(std::move(level));
    }

    return std::nullopt;
}
