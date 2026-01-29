#include "core/storage_manager/storage_manager_impl.h"
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

StorageManagerImpl::StorageManagerImpl(SystemContext &systemContext, std::string basePath, int maxLevel)
    : m_basePath{std::move(basePath)}, m_systemContext{systemContext}, m_maxLevel{std::move(maxLevel)} {};

// 1. Hands entries to Level 0 Manager to write the entries into an SSTable.
std::optional<Error> StorageManagerImpl::write(const std::vector<const Entry *> &entryPtrs, int level)
{
    // std::cout << "[StorageManagerImpl.write()]" << std::endl;

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
        // std::cerr << "[StorageManagerImpl.write()] Failed to write SSTable" << std::endl;
        return std::optional<Error>(status.to_string());
    }

    return std::nullopt;
};

std::optional<Entry> StorageManagerImpl::get(const std::string &key) const
{
    // std::cout << "[StorageManagerImpl.get()]" << "\n";

    // search all levels
    for (const auto &levelManager : m_levelManagers)
    {
        std::optional<Entry> entry{levelManager->getKey(key)};
        if (entry && !entry->tombstone)
        {
            // std::cout << "[StorageManagerImpl.get()] FOUND: " << entry.value() << "\n";
            return entry;
        }
        if (entry && entry->tombstone)
        {
            break;
        }
    }

    // std::cout << "[StorageManagerImpl.get()] key does not exist on disk" << "\n";
    return std::nullopt;
};

std::optional<Error> StorageManagerImpl::compact()
{
    if (m_levelManagers.size() == 0)
        return std::nullopt;

    // merge level `i` with level `i+1`, for all 0 <= i <= n-1
    for (int lvl = 0; lvl < m_maxLevel; lvl++)
    {
        m_levelManagers[lvl]->compactInto(*m_levelManagers[lvl + 1]);
    }

    return std::nullopt;
};

std::optional<Error> StorageManagerImpl::initLevels()
{
    std::cout << "[StorageManagerImpl::initLevels()]" << std::endl;

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
        if (levelNum > m_maxLevel)
        {
            std::cout << "[StorageManagerImpl::initLevels()] Ignoring level-"
                      << levelNum << " (exceeds MAX_LEVEL)" << std::endl;
            continue;
        }

        discoveredLevels.emplace(levelNum, entry.path());
    }

    // Step 2: create LevelManagers from 0 .. MAX_LEVEL
    m_levelManagers.clear(); // clear JIC we currently have some populated state
    m_levelManagers.reserve(m_maxLevel + 1);

    for (int levelNum = 0; levelNum <= m_maxLevel; ++levelNum)
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
            // New level → create directory here (StorageManager responsibility)
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
