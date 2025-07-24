#include "mem_table_impl.h"
#include <optional>

MemTableImpl::MemTableImpl(int size, SkipList &skipList, SSTableManager &ssTableManager) : m_size{size}, m_skiplist{skipList}, m_ssTableManager{ssTableManager} {}; // constructor

Error* MemTableImpl::put(const std::string &key, const std::string &val) {
    // insert into MemTable
    // flush if needed
    if (isReadOnly()) {
        std::cout << "[Memtable.put()] Cannot PUT: read only" << "\n";
        return new Error("[MemtableImpl.put()] Cannot PUT: read only");
    }

    std::cout << "[MemTable.put()] PUTTING..." << "\n"; 

    m_skiplist.set(Entry(key, val));

    if (m_skiplist.getLength() == m_size) {
        m_readOnly = true;
        flushToDisk();
    }

    return nullptr;

    // std::cout << "[MemTableImpl.put()] all items in memtable: " << "\n";
    // for (const Entry )

    // flush to disk
    // just pass in all entries in m_skiplist.getAll()

};

const Entry* MemTableImpl::get(const std::string &key) const {
    std::cout << "[MemTableImpl.get()] GET";
    const Entry* ptr {m_skiplist.get(key)};

    if (ptr == nullptr || (*ptr).tombstone == true) {
        std::cout << "[MemTableImpl.get()] key does not exist in memtable" << "\n";
        return nullptr;
    }

    std::cout << "[MemTableImpl.get()] GOT: (" << key << ", " << (*ptr).val << ")" << "\n";
    return ptr;
};

void MemTableImpl::del(const std::string &key) {
    std::cout << "[MemTableImpl.del()]" << std::endl;

    // if isReadOnly(), it's currently being flushed. So we shouldn't do anything to it.
    // mark as tombstone
    if (isReadOnly()) {
        m_readOnly = true;
        std::cout << "[Memtable.put()] Cannot DELETE: memtable in read-only mode (ie. it is being flushed)" << "\n";
        return;
    }

    m_skiplist.set(Entry(key, "val", true));

    if (m_skiplist.getLength() == m_size) {
        flushToDisk();
    }
};

bool MemTableImpl::isReadOnly() {
    return m_readOnly; 
}

void MemTableImpl::flushToDisk() {
    std::cout << "[MemTableImpl.flushToDisk()]" << std::endl;
    m_readOnly = true;
    
    // obtain every guy in skiplist IN ORDER.
    // gather this in a vector or smt and pass that vector into your SSTable builder
    std::vector<const Entry*> entryPtrs {m_skiplist.getAll()};

    for (const auto& ptr : entryPtrs) {
        std::string key {(*ptr).key};
        std::string val {(*ptr).val};
        bool tombstoneFlag {(*ptr).tombstone};
        std::string tombstone = tombstoneFlag ? "true" : "false";

        std::cout << "(" << key << ", " << val << ", " << tombstone << ")" << std::endl;
    }

    // write to file
    Error* errorPtr { m_ssTableManager.write(entryPtrs) }; // -> writes a new file to level 0.

    if (errorPtr != nullptr) {
        std::cout << "[MemTableImpl.flushToDisk()] ERROR: " << &errorPtr->error << std::endl;
        return;
    }

    // empty skiplist
    m_skiplist.clear();
    
    m_readOnly = false;
};