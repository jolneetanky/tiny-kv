#include "core/mem_table/mem_table_impl.h"
#include <optional>
#include "common/log.h"

MemTableImpl::MemTableImpl(int size, SkipList &skipList, StorageManager &storageManager, WAL &wal, SystemContext &systemContext) : m_size{size}, m_skiplist{skipList}, m_storageManager{storageManager}, m_wal{wal}, m_systemContext{systemContext} {}; // constructor

std::optional<Error> MemTableImpl::put(const std::string &key, const std::string &val)
{
    // insert into MemTable
    // flush if needed
    if (isReadOnly())
    {
        TINYKV_LOG("[MemTableImpl.put()] Cannot PUT: read only");
        return Error{"[MemTableImpl.put()] Cannot PUT: memtable in read-only mode"};
    }

    // If current length exceeds memtable size, set to read-only and flush.
    if (m_skiplist.getLength() == m_size)
    {
        m_readOnly = true;
        std::optional<Error> errOpt{flushToDisk()};

        if (errOpt)
        {
            TINYKV_LOG("[MemTableImpl.put()] Failed to PUT: " << errOpt->error);
            return Error{"[MemTableImpl.put()] Failed to PUT: Flushing error "};
        }
    }

    // Insert in skiplist
    // Write to WAL
    if (const auto &err = m_wal.append(Entry(key, val)))
    {
        TINYKV_LOG("[MemTableImpl.put()] Failed to append to WAL: " << err->error);
        return Error{"MemTableImpl.put()] Failed to PUT: Failed to write to WAL"};
    }

    m_skiplist.set(Entry(key, val));

    return std::nullopt;
};

std::optional<Entry> MemTableImpl::get(const std::string &key) const
{
    TINYKV_LOG("[MemTableImpl.get()] GET");

    std::optional<Entry> optEntry{m_skiplist.get(key)};

    if (!optEntry)
    {
        TINYKV_LOG("[MemTableImpl.get()] key does not exist in memtable");
        return std::nullopt;
    }

    TINYKV_LOG("[MemTableImpl.get()] GOT: (" << key << ", " << optEntry.value().val << ")");
    return optEntry;
};

std::optional<Error> MemTableImpl::del(const std::string &key)
{
    TINYKV_LOG("[MemTableImpl.del()]");

    // TODO: check that entry is either in memtable OR in disk
    if (!get(key) && !m_storageManager.get(key))
    {
        TINYKV_LOG("[MemTableImpl.del()] Cannot DELETE: key does not exist");
        return Error{"Cannot DELETE: key does not exist"};
    }

    // if isReadOnly(), it's currently being flushed. So we shouldn't do anything to it.
    // mark as tombstone
    if (isReadOnly())
    {
        TINYKV_LOG("[MemTableImpl.del()] Cannot DELETE: memtable in read-only mode (ie. it is being flushed)");
        return Error{"Cannot DELETE: memtable in read-only mode"};
    }

    // If current length exceeds memtable size, set to read-only and flush.
    if (m_skiplist.getLength() == m_size)
    {
        m_readOnly = true;
        std::optional<Error> errOpt{flushToDisk()};

        if (errOpt)
        {
            TINYKV_LOG("[MemTableImpl.del()] Failed to DELETE: " << errOpt->error);
            return Error{"[MemTableImpl.put()] Failed to DELETE: Flushing error "};
        }
    }

    // write to WAL
    if (const auto &err = m_wal.append(Entry(key, "val", true)))
    {
        TINYKV_LOG("[MemTableImpl.put()] Failed to append to WAL: " << err->error);
        return Error{"MemTableImpl.put()] Failed to PUT: Failed to write to WAL"};
    }

    // insert tombstone entry into memtable
    m_skiplist.set(Entry(key, "val", true));

    if (!m_skiplist.getLength())
    {
        TINYKV_LOG("[MemTableImpl.del()] Failed to get length of skip list");
        return Error{"Failed to DELETE"};
    }

    return std::nullopt;
};

bool MemTableImpl::isReadOnly() const
{
    return m_readOnly;
}

std::optional<Error> MemTableImpl::flushToDisk()
{
    TINYKV_LOG("[MemTableImpl.flushToDisk()]");
    m_readOnly = true;

    std::optional<std::vector<Entry>> optEntries{m_skiplist.getAll()};

    if (!optEntries)
    {
        TINYKV_LOG("[MemTableImpl.flushToDisk()] Failed to get all entries from memtable");
        return Error{"Failed to get all entries from memtable"};
    }

    std::vector<const Entry *> entryPtrs; // the pointers point to entries in the stack omg...

    // obtain every guy in skiplist IN ORDER.
    // gather this in a vector or smt and pass that vector into your SSTable builder
    for (const Entry &entry : optEntries.value())
    {
        entryPtrs.push_back(&entry);

        TINYKV_LOG(entry);
    }

    // write to file
    std::optional<Error> errOpt{m_storageManager.write(entryPtrs, 0)}; // writes a new file to level 0.

    if (errOpt)
    {
        TINYKV_LOG("[MemTableImpl.flushToDisk()] ERROR: " << errOpt->error);
        return errOpt.value();
    }

    // delete the old WAL file
    if (const auto &err = m_wal.remove())
    {
        TINYKV_LOG("[MemTableImpl.flushToDisk()] ERROR: Failed to delete WAL - " << err->error);
        return err.value();
    }

    // empty skiplist
    m_skiplist.clear();

    m_readOnly = false;

    return std::nullopt;
};

std::optional<Error> MemTableImpl::replayWal()
{
    TINYKV_LOG("[MemTableImpl.replayWal()]");
    const auto &entries = m_wal.getEntries();
    m_readOnly = true;

    // read each entry to memtable
    for (const auto &entry : entries.value())
    {
        if (m_skiplist.getLength() == m_size)
        {
            flushToDisk();
        }

        m_skiplist.set(entry);
    }

    // flush to disk if memtable is now not empty
    if (m_skiplist.getLength() > 0)
    {
        if (const auto &err = flushToDisk())
        {
            TINYKV_LOG("[MemTableImpl.replayWal()] Failed to flush to disk: " << err->error);
            return Error{"[MemTableImpl.replayWal()] Failed to replay WAL: Flushing error "};
        }
    }

    // delete WAL
    m_wal.remove();

    m_readOnly = false;
    return std::nullopt;
};