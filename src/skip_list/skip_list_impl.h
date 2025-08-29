#ifndef SKIP_LIST_IMPL_H
#define SKIP_LIST_IMPL_H

#include <string>
#include <vector>
#include "../types/entry.h"
#include "skip_list.h"
#include <map>
#include <iostream>

// Remember each time we are always only appending entries to our skiplist.
// no updates or wtv.

class SkipListImpl : public SkipList{
    private:
        // TODO: implement your own skiplist. for now im using std::map lol
        std::map<std::string, Entry> m_map;

    public:
        std::optional<Error> set(Entry const &entry) override;
        std::optional<Entry> get(const std::string& key) const override;
        std::optional<std::vector<Entry>> getAll() const override;
        std::optional<Error> clear() override;
        std::optional<int> getLength() const override;
};

#endif