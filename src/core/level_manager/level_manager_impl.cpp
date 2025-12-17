#include "core/level_manager/level_manager_impl.h"
#include "core/sstable_manager/sstable_manager_impl.h"
#include <iostream>
#include <filesystem>

LevelManagerImpl::LevelManagerImpl(int levelNum, std::string directoryPath, SystemContext &systemContext) : m_levelNum{levelNum}, m_directoryPath{directoryPath}, m_systemContext{systemContext} {};

const int &LevelManagerImpl::getLevel()
{
    return m_levelNum;
}

std::optional<Error> LevelManagerImpl::writeFile(const std::vector<const Entry *> &entries)
{
    std::cout << "[LevelManagerImpl.writeFile()]" << std::endl;

    // construct an SSTableFileManager, initialized with entries
    // they are automatically written to disk in the ctor (as per semantics of SSTableFileManager)
    m_ssTableManagers.push_back(std::make_unique<SSTableManagerImpl>(m_directoryPath, m_systemContext, entries));

    return std::nullopt;
};

std::optional<Entry> LevelManagerImpl::searchKey(const std::string &key)
{
    std::cout << "[LevelManagerImpl.searchKey()] LEVEL " << std::to_string(m_levelNum) << "\n";

    // read from back to front
    // because we wanna read in LIFO order for level 0
    // last inserted == newest!
    // for (const auto &fileManager : m_ssTableManagers)
    for (int i = m_ssTableManagers.size() - 1; i >= 0; i--)
    {
        const auto &fileManager{m_ssTableManagers[i]};

        if (!fileManager->contains(key))
        {
            continue;
        }

        std::optional<Entry> entryOpt{fileManager->get(key)};
        if (entryOpt)
        {
            std::cout << "[LevelManagerImpl.searchKey()] FOUND" << "\n";
            return entryOpt;
        }

        // in the latest entry, key has been deleted. So can stop searching alr
        if (entryOpt && entryOpt->tombstone)
        {
            break;
        }
    }

    std::cout << "[LevelManagerImpl.searchKey()] key does not exist on disk" << "\n";
    return std::nullopt;
};

std::pair<LevelManagerImpl::const_iterator, LevelManagerImpl::const_iterator> LevelManagerImpl::getFiles()
{
    std::cout << "LevelManagerImpl.getFiles()]" << "\n";
    return {m_ssTableManagers.begin(), m_ssTableManagers.end()};
};

std::optional<Error> LevelManagerImpl::deleteFiles(std::vector<const SSTableManager *> files)
{
    std::cout << "[LevelManagerImpl.deleteFiles()]" << "\n";

    for (const auto &file : files)
    {
        std::string fullPath = file->getFullPath();

        // 1. Delete file from disk
        if (!std::filesystem::remove(fullPath))
        {
            return Error{"Failed to delete file"};
        }

        // 2. Remove the file from m_fileManagers
        auto it = std::remove_if(
            m_ssTableManagers.begin(),
            m_ssTableManagers.end(),
            [&](const std::unique_ptr<SSTableManager> &ptr)
            {
                // remove if path of file matches that that we are tryna remove
                return ptr && ptr->getFullPath() == fullPath;
            });

        if (it != m_ssTableManagers.end())
        {
            m_ssTableManagers.erase(it, m_ssTableManagers.end());
        }
    }

    return std::nullopt;
};

/*
1. Creates the directory for this level if it doesn't exist
2. Look through existing files (ie. SSTables) in this level, loads their manager into memory
*/
std::optional<Error> LevelManagerImpl::init()
{
    std::cout << "[LevelManagerImpl.init()]" << "\n";

    // 1. Ceate directory if it doesn't exist
    if (!std::filesystem::exists(m_directoryPath))
    {
        try
        {
            if (!std::filesystem::create_directories(m_directoryPath))
            {
                return Error{"Failed to create directory: " + m_directoryPath};
            }
            std::cout << "[LevelManagerImpl.init()] Created directory: " << m_directoryPath << "\n";
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            return Error{std::string("Filesystem error creating directory: ") + e.what()};
        }
    }

    // 2. Look thorugh existing files in the directory for this level, and initialize them
    for (auto const &dirEntry : std::filesystem::directory_iterator{m_directoryPath})
    {
        const std::string &fileName = dirEntry.path().filename().string();
        auto fileManager = std::make_unique<SSTableManagerImpl>(m_directoryPath, fileName, m_systemContext);

        m_ssTableManagers.push_back(std::move(fileManager));
    }

    return std::nullopt;
};