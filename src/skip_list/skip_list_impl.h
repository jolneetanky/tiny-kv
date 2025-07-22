#ifndef SKIP_LIST_IMPL_H
#define SKIP_LIST_IMPL_H

#define JHR_SKIP_LIST_IMPLEMENTATION // in order to use it
#include "../external/jhr_skip_list.hpp"

#include <string>
#include <vector>
#include "../types/entry.h"
#include "skip_list.h"
#include <map>

// Remember each time we are always only appending entries to our skiplist.
// no updates or wtv.

class SkipListImpl : public SkipList{
    private:
        // jhr::Skip_List<Entry> m_skiplist;
        // TODO: implement your own skiplist. for now im using std::map lol
        std::map<std::string, Entry> m_map;

    public:
        void set(Entry const &entry) override;
        const Entry* get(const std::string& key) override;
        std::vector<const Entry*> getAll() const override;
        void clear() override; // NOTE: after calliing clear, all pointers to `Entry` in the skiplist become invalid.
        int getLength() override;
};

#endif