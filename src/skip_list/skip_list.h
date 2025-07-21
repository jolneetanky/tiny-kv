#ifndef SKIP_LIST_H
#define SKIP_LIST_H

#include <string>
#include <vector>
#include "../types/entry.h"

// Remember each time we are always only appending entries to our skiplist.
// no updates or wtv.

class SkipList {
    private:
        // int m_size;

    public:
        // SkipList(int size);
        void set(Entry const &entry);
        const Entry& get(const std::string& key);
        std::vector<Entry> getAll() const;
        int getLength();
};

#endif