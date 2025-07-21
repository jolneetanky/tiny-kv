#include "skip_list.h"

// SkipList::SkipList(int size) : m_size{size} {};
void SkipList::set(Entry const &entry) {

}

const Entry& SkipList::get(const std::string& key) {
    static Entry dummy("key", "val"); // lifetime extends for the duration of the entire program
    return dummy;
}

std::vector<Entry> SkipList::getAll() const {
    return std::vector<Entry>();
}

int SkipList::getLength() {
    return 1;
}