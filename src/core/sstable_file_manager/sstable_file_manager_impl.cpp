#include "core/sstable_file_manager/sstable_file_manager_impl.h"
#include <iostream>
#include "core/bloom_filter/bloom_filter_impl.h"

// CTORS
/*
1. Creates file it it doesn't exist (because an existence of this instance ust be tied to an actual file, empty or not)
*/
SSTableFileManagerImpl::SSTableFileManagerImpl(std::string directoryPath, SystemContext &systemCtx, const std::vector<const Entry *> &entries) : m_directoryPath{directoryPath}, m_systemContext{systemCtx}, m_bloomFilter{std::make_unique<BloomFilterImpl>(1000, 7)}
{
    m_file_number = m_systemContext.file_number_allocator.next();
    // create file if it doesn't exist
    std::string fname = _generateSSTableFileName();
    std::string fullPath{m_directoryPath + "/" + fname};
    m_fullPath = fullPath;
    _createFileIfNotExists(fullPath);
    _writeEntriesToFile(entries, fullPath);
}

// TODO: set file number
SSTableFileManagerImpl::SSTableFileManagerImpl(const std::string &directoryPath, const std::string &fileName, SystemContext &systemCtx) : m_directoryPath{directoryPath}, m_fname{fileName}, m_fullPath{directoryPath + "/" + fileName}, m_systemContext{systemCtx}, m_bloomFilter{std::make_unique<BloomFilterImpl>(1000, 7)} {};

// returns the first entry found - tombstone or not.
// else sometimes the (most recent) entry has been found and it's tombstoned,
// but because we tell the caller not found, they continue searching in the other files.
std::optional<Entry> SSTableFileManagerImpl::get(const std::string &key)
{
    std::cout << "[SSTableFileManagerImpl.get()]" << "\n";

    // initialize if needed
    if (!m_initialized)
    {
        std::optional<Error> err{_init()};
        if (err)
        {
            std::cerr << "[SSTableFileManagerImpl.get()] Failed to read file to memory: " << err->error << "\n";
            return std::nullopt;
        }
    }

    const std::vector<Entry> &entries = m_ssTableFile->entries;

    if (entries.size() == 0)
    {
        return std::nullopt;
    }

    for (auto &entry : entries)
    {
        std::cout << "[SSTableFileManagerImpl.get()] (" << entry.key << ", " << entry.val << ", " << entry.tombstone << ")" << "\n";
    }

    // binary search :)
    int l = 0;
    int r = entries.size() - 1;

    // assume within an SSTable there are no duplicate keys.
    while (l < r)
    {
        int mid = l + ((r - l) / 2);
        const Entry &midEntry = entries[mid];

        if (midEntry.key == key)
        {
            std::cout << "[SSTableFileManagerImpl.get()] FOUND: " << "\n";
            return midEntry;
        }

        if (key < midEntry.key)
        {
            r = mid - 1;
        }
        else
        {
            l = mid + 1;
        }
    }

    if (entries[l].key == key)
    {
        std::cout << "[SSTableFileManagerImpl.get()] FOUND" << "\n";
        return entries[l];
    }

    std::cout << "[SSTableFileManagerImpl.get()] NOT FOUND" << "\n";
    return std::nullopt;
};

uint64_t SSTableFileManagerImpl::getFileNumber()
{
    return this->m_file_number;
}

std::optional<TimestampType> SSTableFileManagerImpl::getTimestamp()
{
    if (!m_initialized)
    {
        std::optional<Error> err{_init()};
        if (err)
        {
            std::cerr << "[SSTableFileManagerImpl.getTimestamp()] Failed to read file to memory: " << err->error << "\n";
            return std::nullopt;
        }
    }

    return m_ssTableFile->timestamp;
};

std::optional<std::vector<Entry>> SSTableFileManagerImpl::getEntries()
{
    if (!m_initialized)
    {
        std::optional<Error> err{_init()};
        if (err)
        {
            std::cerr << "[SSTableFileManagerImpl.getEntries()] Failed to read file to memory: " << err->error << "\n";
            return std::nullopt;
        }
    }

    return m_ssTableFile->entries;
};

std::string SSTableFileManagerImpl::getFullPath() const
{
    return m_fullPath;
};

std::optional<std::string> SSTableFileManagerImpl::getStartKey()
{
    if (!m_initialized)
    {
        _init();
    }

    if (m_ssTableFile->entries.size() == 0)
    {
        return std::nullopt;
    }

    return m_ssTableFile->entries[0].key;
};

std::optional<std::string> SSTableFileManagerImpl::getEndKey()
{
    if (!m_initialized)
    {
        std::cout << "[SSTableFileManager.getEndKey()] Failed to get start key: file manager not yet initialized" << "\n";
        return std::nullopt;
    }

    if (m_ssTableFile->entries.size() == 0)
    {
        return std::nullopt;
    }

    return m_ssTableFile->entries.back().key;
};

bool SSTableFileManagerImpl::contains(std::string key)
{
    std::cout << "[SSTableFileManagerImpl.contains()]" << std::endl;
    // check bloom filter if the entry exists
    if (!m_initialized)
    {
        std::optional<Error> err{_init()};
        if (err)
        {
            std::cerr << "[SSTableFileManagerImpl.get()] Failed to read file to memory: " << err->error << "\n";
            return false;
        }
    }

    return m_bloomFilter->contains(key);
};