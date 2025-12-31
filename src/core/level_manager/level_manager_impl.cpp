#include "core/level_manager/level_manager_impl.h"
#include "core/sstable_manager/sstable_manager_impl.h"
#include "core/sstable_manager/sstable_writer.h"
#include "core/sstable_manager/sstable_reader.h"
#include <iostream>
#include <filesystem>

LevelManagerImpl::LevelManagerImpl(int levelNum, std::string directoryPath, SystemContext &systemContext) : m_levelNum{levelNum}, m_directoryPath{directoryPath}, m_systemContext{systemContext} {};

const int &LevelManagerImpl::getLevel()
{
    return m_levelNum;
}

std::string LevelManagerImpl::_generateSSTableFileName() const
{
    static std::atomic<uint64_t> counter{0};

    auto now = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                  now.time_since_epoch())
                  .count();

    uint64_t uniqueId = (ns << 16) ^ counter.fetch_add(1); // mix counter + time

    return "table-" + std::to_string(uniqueId);
}

TimestampType LevelManagerImpl::_getTimeNow()
{
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();

    return timestamp;
}

std::optional<Error> LevelManagerImpl::writeFile(const std::vector<const Entry *> &entryPtrs)
{
    std::cout << "[LevelManagerImpl.writeFile()]" << std::endl;

    SSTableWriter writer;
    std::string full_path = m_directoryPath + _generateSSTableFileName();
    TimestampType timestamp = _getTimeNow();
    FileNumber file_num = m_systemContext.file_number_allocator.next();

    // std::vector<Entry> entries;
    // entries.reserve(entryPtrs.size());
    // for (const auto &ptr : entryPtrs)
    //     entries.emplace_back(ptr);

    // SSTableMetadata metadata = writer.write(full_path, entries, timestamp, file_num);

    // m_ss_tables.emplace_back(metadata);

    // construct an SSTableFileManager, initialized with entries
    // they are automatically written to disk in the ctor (as per semantics of SSTableFileManager)
    m_ssTableManagers.push_back(std::make_unique<SSTableManagerImpl>(m_directoryPath, m_systemContext, entryPtrs));

    return std::nullopt;
};

std::optional<Entry> LevelManagerImpl::searchKey(const std::string &key)
{
    std::cout << "[LevelManagerImpl.searchKey()] LEVEL " << std::to_string(m_levelNum) << "\n";

    // read from back to front
    // because we wanna read in LIFO order for level 0
    // last inserted == newest!
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

    // 2. Look through existing files in the directory for this level, and initialize them
    for (auto const &dirEntry : std::filesystem::directory_iterator{m_directoryPath})
    {
        const std::string &fileName = dirEntry.path().filename().string();

        // read the files
        SSTableReader reader;
        std::unique_ptr<SSTable> table = std::make_unique<SSTable>(reader.read(dirEntry.path().string()));
        m_ssTables.emplace_back(std::move(table)); // emplace the rvalue, which calls the move ctor of SSTable.

        // TODO: delete this
        auto fileManager = std::make_unique<SSTableManagerImpl>(m_directoryPath, fileName, m_systemContext);

        m_ssTableManagers.push_back(std::move(fileManager));
    }

    return std::nullopt;
};

// NEW API
std::optional<Entry> LevelManagerImpl::getKey(const std::string &key)
{
    std::cout << "[LevelManagerImpl.getKey()] LEVEL " << std::to_string(m_levelNum) << "\n";

    // read from back to front
    // because we wanna read in LIFO order for level 0
    // last inserted == newest!
    for (const auto &ssTable : m_ssTables)
    {
        if (!ssTable->contains(key))
        {
            continue;
        }

        std::optional<Entry> entryOpt{ssTable->get(key)};
        if (entryOpt)
        {
            std::cout << "[LevelManagerImpl.getKey()] FOUND" << "\n";
            return entryOpt;
        }

        // in the latest entry, key has been deleted. So can stop searching alr
        if (entryOpt && entryOpt->tombstone)
        {
            break;
        }
    }

    std::cout << "[LevelManagerImpl.getKey()] key does not exist on disk" << "\n";
    return std::nullopt;
};

Status LevelManagerImpl::createTable(std::vector<Entry> &&entries)
{
    std::cout << "[LevelManagerImpl.createTable()]" << std::endl;

    SSTableWriter writer;
    std::string full_path = m_directoryPath + _generateSSTableFileName();
    TimestampType timestamp = _getTimeNow();
    FileNumber file_num = m_systemContext.file_number_allocator.next();

    SSTableMetadata metadata = writer.write(full_path, entries, timestamp, file_num);
    std::unique_ptr<SSTable> table = std::make_unique<SSTable>(metadata, std::move(entries));

    m_ssTables.emplace_back(std::move(table)); // calls move ctor `unique_ptr(unique_ptr&&)`, same as if we did push_back. No diff here.

    return Status::OK();
};

std::span<const SSTable *const> LevelManagerImpl::getTables()
{
}

Status LevelManagerImpl::deleteTables(std::span<const SSTable *>){

}; // delete based on tableID for this particular level. We can expose SSTable.getId

Status LevelManagerImpl::initNew()
{
    std::cout << "[LevelManagerImpl.initNew()]" << "\n";

    // 1. Look through existing files in the directory for this level, and initialize them
    for (auto const &dirEntry : std::filesystem::directory_iterator{m_directoryPath})
    {
        const std::string &fileName = dirEntry.path().filename().string();

        // read the files
        SSTableReader reader;
        std::unique_ptr<SSTable> table = std::make_unique<SSTable>(reader.read(dirEntry.path().string()));
        m_ssTables.emplace_back(std::move(table)); // emplace the rvalue, which calls the move ctor of SSTable.

        // TODO: delete this
        auto fileManager = std::make_unique<SSTableManagerImpl>(m_directoryPath, fileName, m_systemContext);

        m_ssTableManagers.push_back(std::move(fileManager));
    }

    Status::OK();
};