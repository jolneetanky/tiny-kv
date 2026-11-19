#include "core/storage_manager/storage_manager_impl.h"
#include "core/level_manager/level_manager_impl.h"
#include "core/level_manager/compaction_planner.h"
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
#include "common/log.h"

StorageManagerImpl::StorageManagerImpl(SystemContext &systemContext, std::string basePath, int maxLevel, std::unique_ptr<const TableFormat> tableFormat)
    : m_basePath{std::move(basePath)}, m_systemContext{systemContext}, m_tableFormat{std::move(tableFormat)}, m_maxLevel{std::move(maxLevel)} {};

// 1. Hands entries to Level 0 Manager to write the entries into an SSTable.
std::optional<Error> StorageManagerImpl::write(tinykv::Iterator &entries, int level)
{
    // initialise levels if there are no level managers read into memory yet.
    if (m_levelManagers.size() == 0)
    {
        initLevels();
    }

    // Write to level 0
    auto &level0Manager{m_levelManagers[0]};

    Status status{level0Manager->createTable(entries)};
    if (!status.ok())
    {
        // std::cerr << "[StorageManagerImpl.write()] Failed to write SSTable" << std::endl;
        return std::optional<Error>(status.to_string());
    }

    return std::nullopt;
};

std::optional<Entry> StorageManagerImpl::get(const std::string &key) const
{
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
    if (m_levelManagers.empty())
        return std::nullopt;

    TINYKV_LOG("[StorageManagerImpl::compact()] plan and compact adjacent levels");

    // Compact from the upper levels toward the bottom. A completed pass from
    // level N to N+1 can therefore feed its output into the next pair.
    for (int sourceLevelNumber = 0; sourceLevelNumber < m_maxLevel; ++sourceLevelNumber)
    {
        LevelManager &source = *m_levelManagers[sourceLevelNumber];
        LevelManager &destination = *m_levelManagers[sourceLevelNumber + 1];

        // collect metadata of tables, get grouprs
        const std::vector<SSTableMetadata> sourceTables = source.tableMetadata();
        const std::vector<SSTableMetadata> destinationTables = destination.tableMetadata();
        const std::vector<tinykv::CompactionPlan> plans =
            tinykv::CompactionPlanner::planOverlappingGroups(sourceTables, destinationTables);

        TINYKV_LOG("[StorageManagerImpl::compact()] level-" + std::to_string(sourceLevelNumber) +
                   " -> level-" + std::to_string(sourceLevelNumber + 1) +
                   ": " + std::to_string(plans.size()) + " overlap group(s)");

        for (const tinykv::CompactionPlan &plan : plans)
        {
            Status status = source.compactInto(destination, plan);
            if (!status.ok())
            {
                return Error{
                    "Failed to compact level-" + std::to_string(sourceLevelNumber) +
                    " into level-" + std::to_string(sourceLevelNumber + 1) +
                    ": " + status.to_string()};
            }
        }
    }

    return std::nullopt;
};

bool StorageManagerImpl::isCompactionNeeded() const
{
    constexpr size_t kLevelZeroTableLimit = 4;
    return !m_levelManagers.empty() && m_levelManagers.front()->tableCount() >= kLevelZeroTableLimit;
}

std::optional<Error> StorageManagerImpl::initLevels()
{
    TINYKV_LOG("[StorageManagerImpl::initLevels()]");

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
            m_systemContext,
            *m_tableFormat);

        Status status;
        // 2 CASES:
        // 1. either the `levelNum` has alr been discovered (ie. is an existing directory), OR
        // 2. `levelNum` has not yet been discovered (ie. the corresponding directory doesn't exist yet)
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
