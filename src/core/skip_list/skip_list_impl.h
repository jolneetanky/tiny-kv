#ifndef SKIP_LIST_IMPL_H
#define SKIP_LIST_IMPL_H

#include <string>
#include <vector>
#include "types/entry.h"
#include "core/skip_list/skip_list.h"
#include <map>
#include <iostream>

// Remember each time we are always only appending entries to our skiplist.
// no updates or wtv.

class SkipListImpl : public SkipList
{
private:
    class Iter final : public tinykv::Iterator
    {
    public:
        explicit Iter(const std::map<std::string, Entry> *map);

        bool Valid() const override;
        void SeekToFirst() override;
        void SeekToLast() override;
        void Seek(const std::string &target) override;
        void Next() override;

        // ACCESSORS
        // REQUIRES: Valid()
        const std::string &Key() const override;
        const std::string &Value() const override;
        bool isTombstone() const override;

    private:
        const std::map<std::string, Entry> *m_map;
        std::map<std::string, Entry>::const_iterator m_it; // use the iterator exposed by std::map
    };

    // TODO: implement your own skiplist. for now im using std::map lol
    std::map<std::string, Entry> m_map;

public:
    std::optional<Error> set(Entry const &entry) override;
    std::optional<Entry> get(const std::string &key) const override;
    std::optional<std::vector<Entry>> getAll() const override;
    std::unique_ptr<tinykv::Iterator> NewIterator() const override;
    std::optional<Error> clear() override;
    std::optional<int> getLength() const override;
};

#endif
