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

    for (int lvl = 0; lvl < m_levelManagers.size() - 1; lvl++)
    {
        m_levelManagers[lvl]->compactInto(*m_levelManagers[lvl + 1]);
        // 1. For every SSTable in this levelManager, pass it to `levelManager[lvl+1]`, and ask it to overwrite its own overlapping file with these entries
        // 2. Even better, do something like `levelManager[lvl].compactInto(levelManager[lvl+1])`
    }

    return std::nullopt;
};

// initializes the level managers based on existing folders on disk. Creates level 0 file manager if there's nothing
std::optional<Error> DiskManagerImpl::initLevels()
{
    std::cout << "[DiskManagerImpl::initLevels()]" << std::endl;

    const std::string &basePath = m_basePath;
    const std::string &level0Path = basePath + "/level-0";
    std::vector<std::pair<int, std::filesystem::path>> levelDirs;

    // create directory ./sstables/level-0 if it does not alr exist
    try
    {
        if (std::filesystem::create_directories(level0Path))
        {
            std::cout << "[DiskManagerImpl::initLevels()] Successfully created directory " << level0Path << std::endl;
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "[DiskManagerImpl::initLevels()] Error creating directories: " << e.what() << "\n";
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
        auto level = std::make_unique<LevelManagerImpl>(levelNum, path.string(), m_systemContext);
        Status status = level->initNew();
        // initialize level with files
        if (!status.ok())
        {
            return Error(status.to_string());
        }

        m_levelManagers.push_back(std::move(level)); // destroys the local `level` after copying the value
    }

    return std::nullopt;
};