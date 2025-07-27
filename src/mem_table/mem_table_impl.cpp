#include "mem_table_impl.h"
#include <optional>

MemTableImpl::MemTableImpl(int size, SkipList &skipList, SSTableManager &ssTableManager) : m_size{size}, m_skiplist{skipList}, m_ssTableManager{ssTableManager} {}; // constructor

std::optional<Error> MemTableImpl::put(const std::string &key, const std::string &val) {

    // insert into MemTable
    // flush if needed
    if (isReadOnly()) {
        std::cout << "[MemTableImpl.put()] Cannot PUT: read only" << "\n";
        return Error{ "[MemTableImpl.put()] Cannot PUT: memtable in read-only mode" };
    }

    // If current length exceeds memtable size, set to read-only and flush.
    if (m_skiplist.getLength() == m_size) {
        m_readOnly = true;
        std::optional<Error> errOpt {flushToDisk()};
        
        if (errOpt) {
            std::cout << "[MemTableImpl.put()] Failed to PUT: " << errOpt->error << "\n";
            return Error{ "[MemTableImpl.put()] Failed to PUT: Flushing error "};
        }
    }

    // Insert in skiplist
    m_skiplist.set(Entry(key, val));

    return std::nullopt;
};

std::optional<Entry> MemTableImpl::get(const std::string &key) const {
    std::cout << "[MemTableImpl.get()] GET";

    std::optional<Entry> optEntry {m_skiplist.get(key)};

    if (!optEntry || optEntry.value().tombstone) {
        std::cout << "[MemTableImpl.get()] key does not exist in memtable" << "\n";
        return std::nullopt;
    }

    std::cout << "[MemTableImpl.get()] GOT: (" << key << ", " << optEntry.value().val << ")" << "\n";
    return optEntry;
};

std::optional<Error> MemTableImpl::del(const std::string &key) {
    std::cout << "[MemTableImpl.del()]" << std::endl;

    // TODO: check that entry is either in memtable OR in disk
    if (!get(key) && !m_ssTableManager.get(key)) {
        std::cout << "[MemTableImpl.del()] Cannot DELETE: key does not exist" << std::endl;
        return Error { "Cannot DELETE: key does not exist"};
    }

    // if isReadOnly(), it's currently being flushed. So we shouldn't do anything to it.
    // mark as tombstone
    if (isReadOnly()) {
        std::cout << "[MemTableImpl.del()] Cannot DELETE: memtable in read-only mode (ie. it is being flushed)" << std::endl;
        return Error { "Cannot DELETE: memtable in read-only mode" };
    }

    // If current length exceeds memtable size, set to read-only and flush.
    if (m_skiplist.getLength() == m_size) {
        m_readOnly = true;
        std::optional<Error> errOpt {flushToDisk()};
        
        if (errOpt) {
            std::cout << "[MemTableImpl.del()] Failed to DELETE: " << errOpt->error << "\n";
            return Error{ "[MemTableImpl.put()] Failed to DELETE: Flushing error "};
        }
    }

    // insert tombstone entry into memtable
    m_skiplist.set(Entry(key, "val", true));

    if (!m_skiplist.getLength()) {
        std::cout << "[MemTableImpl.del()] Failed to get length of skip list" << "\n";
        return Error { "Failed to DELETE"};
    }

    return std::nullopt;
};

bool MemTableImpl::isReadOnly() const {
    return m_readOnly; 
}

std::optional<Error> MemTableImpl::flushToDisk() {
    std::cout << "[MemTableImpl.flushToDisk()]" << std::endl;
    m_readOnly = true;
    
    std::optional<std::vector<Entry>> optEntries {m_skiplist.getAll()};

    if (!optEntries) {
        std::cout << "[MemTableImpl.flushToDisk()] Failed to get all entries from memtable";
        return Error{ "Failed to get all entries from memtable" };
    }

    std::vector<const Entry*> entryPtrs;

    // obtain every guy in skiplist IN ORDER.
    // gather this in a vector or smt and pass that vector into your SSTable builder
    for (const Entry& entry : optEntries.value()) {
        std::string key {entry.key};
        std::string val {entry.val};
        bool tombstoneFlag {entry.tombstone};
        std::string tombstone = tombstoneFlag ? "true" : "false";

        entryPtrs.push_back(&entry);

        std::cout << "(" << key << ", " << val << ", " << tombstone << ")" << std::endl;
    }

    // write to file
    std::optional<Error> errOpt { m_ssTableManager.write(entryPtrs) }; // writes a new file to level 0.

    if (errOpt) {
        std::cout << "[MemTableImpl.flushToDisk()] ERROR: " << errOpt->error << "\n";
        return errOpt->error;
    }

    // empty skiplist
    m_skiplist.clear();
    
    m_readOnly = false;

    return std::nullopt;
};