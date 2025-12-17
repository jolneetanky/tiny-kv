#include "core/sstable_manager/sstable_manager_impl.h"
#include <iostream>
#include "core/bloom_filter/bloom_filter_impl.h"

// CTORS
/*
This ctor is when we wanna initialize an SSTableManager, using entries that currently exist
1. Writes entries to disk
2. Loads entries into memory

PROBLEM: Too many responsibiliies, it's a little unclear what the responsibilities of this class is.
Sometimes it reads to memory, sometimes it writes to file
We should allow like the outside LevelManager to handle that? Then SSTableManager is simply the in-memory representation of an SSTable.

LevelManager does:
1. Given a list of entries, it serializes them and writes them to disk
2. Represents an SSTableManager using SSTableManager
*/

// OPTIONS:
// IDEA #1
/*
1. SSTableManager only exposes its metadata, which allows us to compare two SSTableManagers
2. LevelManager handles:
- convert entries to file representation
- pass entries to an SSTableManager to construct an in-memory representation
3. To load existing files to memory:
- LevelManager has a `readFilesToMemory` method, which parses the file, then converts the entries and metadata into an SSTableManager.
*/

// IDEA #2: Keep things the same
/*
SSTableManager:
1. Read entries from disk into memory
2. Write entries to disk
3. Expose getEntries to get the entries of an SSTable (in sorted order).

LevelManager:
1. Just passes entries to an SSTableManager to construct it, and the file management is completely abstracted away from LevelManager
 */
SSTableManagerImpl::SSTableManagerImpl(std::string directoryPath, SystemContext &systemCtx, const std::vector<const Entry *> &entries) : m_directoryPath{directoryPath}, m_systemContext{systemCtx}, m_bloomFilter{std::make_unique<BloomFilterImpl>(1000, 7)}
{
    m_file_number = m_systemContext.file_number_allocator.next();
    std::string fname = _generateSSTableFileName();
    std::string fullPath{m_directoryPath + "/" + fname};
    m_fullPath = fullPath;
    _createFileIfNotExists(fullPath);
    _writeEntriesToFile(entries, fullPath);
}

// TODO: set file number
SSTableManagerImpl::SSTableManagerImpl(const std::string &directoryPath, const std::string &fileName, SystemContext &systemCtx) : m_directoryPath{directoryPath}, m_fname{fileName}, m_fullPath{directoryPath + "/" + fileName}, m_systemContext{systemCtx}, m_bloomFilter{std::make_unique<BloomFilterImpl>(1000, 7)} {};

// returns the first entry found - tombstone or not.
// else sometimes the (most recent) entry has been found and it's tombstoned,
// but because we tell the caller not found, they continue searching in the other files.
std::optional<Entry> SSTableManagerImpl::get(const std::string &key)
{
    std::cout << "[SSTableManagerImpl.get()]" << "\n";

    // initialize if needed
    if (!m_initialized)
    {
        std::optional<Error> err{_init()};
        if (err)
        {
            std::cerr << "[SSTableManagerImpl.get()] Failed to read file to memory: " << err->error << "\n";
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
        std::cout << "[SSTableManagerImpl.get()] (" << entry.key << ", " << entry.val << ", " << entry.tombstone << ")" << "\n";
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
            std::cout << "[SSTableManagerImpl.get()] FOUND: " << "\n";
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
        std::cout << "[SSTableManagerImpl.get()] FOUND" << "\n";
        return entries[l];
    }

    std::cout << "[SSTableManagerImpl.get()] NOT FOUND" << "\n";
    return std::nullopt;
};

uint64_t SSTableManagerImpl::getFileNumber()
{
    return this->m_file_number;
}

std::optional<TimestampType> SSTableManagerImpl::getTimestamp()
{
    if (!m_initialized)
    {
        std::optional<Error> err{_init()};
        if (err)
        {
            std::cerr << "[SSTableManagerImpl.getTimestamp()] Failed to read file to memory: " << err->error << "\n";
            return std::nullopt;
        }
    }

    return m_ssTableFile->timestamp;
};

std::optional<std::vector<Entry>> SSTableManagerImpl::getEntries()
{
    if (!m_initialized)
    {
        std::optional<Error> err{_init()};
        if (err)
        {
            std::cerr << "[SSTableManagerImpl.getEntries()] Failed to read file to memory: " << err->error << "\n";
            return std::nullopt;
        }
    }

    return m_ssTableFile->entries;
};

std::string SSTableManagerImpl::getFullPath() const
{
    return m_fullPath;
};

std::optional<std::string> SSTableManagerImpl::getStartKey()
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

std::optional<std::string> SSTableManagerImpl::getEndKey()
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

bool SSTableManagerImpl::contains(std::string key)
{
    std::cout << "[SSTableManagerImpl.contains()]" << std::endl;
    // check bloom filter if the entry exists
    if (!m_initialized)
    {
        std::optional<Error> err{_init()};
        if (err)
        {
            std::cerr << "[SSTableManagerImpl.get()] Failed to read file to memory: " << err->error << "\n";
            return false;
        }
    }

    return m_bloomFilter->contains(key);
};