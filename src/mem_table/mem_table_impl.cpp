#include "mem_table_impl.h"
#include <optional>

MemTableImpl::MemTableImpl(int size, SkipList &skipList) : m_size{size}, m_skiplist{skipList} {}; // constructor

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
    }

    return nullptr;

    // std::cout << "[MemTableImpl.put()] all items in memtable: " << "\n";
    // for (const Entry )

    // flush to disk
    // just pass in all entries in m_skiplist.getAll()

};
const Entry& MemTableImpl::get(const std::string &key) const {
    return m_skiplist.get(key);
};

void MemTableImpl::del(const std::string &key) {
    // mark as tombstone
};

bool MemTableImpl::isReadOnly() {
    return m_readOnly; 
}
void MemTableImpl::flushToDisk() {
    // make memtable read-only

    // flush to disk (not sure how to flush - entyr by entry? batch? idk)
};