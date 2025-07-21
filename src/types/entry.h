#ifndef ENTRY_H
#define ENTRY_H

#include <string>

struct Entry {
    std::string key;
    std::string val;
    bool tombstone = false;

    Entry(std::string k, std::string v, bool t = false) : key{k}, val{v}, tombstone{t} {};

    bool operator<(const Entry &other) const {
        return key < other.key;
    }

    bool operator==(const Entry &other) const {
        return key == other.key;
    }
};

#endif