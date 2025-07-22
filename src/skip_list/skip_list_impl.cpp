#include "skip_list_impl.h"

// SkipList::SkipList(int size) : m_size{size} {};
void SkipListImpl::set(Entry const &entry) {
    std::cout << "[SkipListImpl.set()]" << "\n";
    // delete entries with the same key from the memtable
    // m_skiplist.remove(entry);
    // insert the entry
    // m_skiplist.insert(entry);
    m_map[entry.key] = entry;
}

// return nullptr if not found
// case #1: not found in memtable => return nullptr
// case #2: key exists in memtable, but entry.tombstone == true => return nullptr
// case #3: key exists in memtable, and entry.tombstone == false => return entry
const Entry* SkipListImpl::get(const std::string& key) {
    std::cout << "[SkipListImpl.get()]" << std::endl;
    // const Entry* ptr = m_skiplist.find(Entry(key, "val"));
    auto it = m_map.find(key);

    if (it != m_map.end()) {
        Entry entry{it->second};
        if (entry.tombstone == false) {
            return &it->second;
        }
    }

    return nullptr;
}

std::vector<const Entry*> SkipListImpl::getAll() const {
    std::cout << "SkipListImpl.getAll()" << std::endl;
    std::vector<const Entry*> res;
    for (const auto& [key, entry] : m_map) {
        std::cout << "HII" << entry.val << std::endl;
        res.push_back(&entry);
    }
    return res;
}

int SkipListImpl::getLength() {
    return m_map.size();
}

void SkipListImpl::clear() {
    std::cout << "SkipListImpl.clear()" << std::endl;
    m_map.clear();
};