#ifndef ENTRY_H
#define ENTRY_H

#include <string>
#include <ostream>
#include "timestamp.h"
#include <functional>

struct Entry
{
    std::string key;
    std::string val;
    bool tombstone = false;

    Entry() = default; // added jic some guys try to default construct an Entry (eg. std::map)
    Entry(std::string k, std::string v, bool t = false) : key{k}, val{v}, tombstone{t} {};

    bool operator<(const Entry &other) const
    {
        return key < other.key;
    }

    bool operator==(const Entry &other) const
    {
        return key == other.key;
    }
};

// make it inline so only one definition exists across TUs
// because we're defining this here in the header file
inline std::ostream &operator<<(std::ostream &os, const Entry &e)
{
    os << "(" << e.key << ", " << e.val << ", " << (e.tombstone == 1 ? "true" : "false") << ")";
    return os;
}

// define `std::hash<Entry>` for custom comparator
namespace std
{
    template <> // tell compiler that this is a specialization of an existing template (ie. `std::hash<>`)
    struct hash<Entry>
    {
        size_t operator()(const Entry &e) const noexcept
        {
            return std::hash<std::string>{}(e.key);
        }
    };
}

#endif
