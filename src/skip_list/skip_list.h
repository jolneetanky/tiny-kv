#ifndef SKIP_LIST_H
#define SKIP_LIST_H

#include <string>
#include <vector>
#include "../types/entry.h"

// Remember each time we are always only appending entries to our skiplist.
// no updates or wtv.

class SkipList {
    public:
        virtual void set(Entry const &entry) = 0;
        virtual const Entry* get(const std::string& key) = 0;
        // Tells caller that each Entry in this vector cannot be modified.
        virtual std::vector<const Entry*> getAll() const = 0;
        virtual void clear() = 0;
        virtual int getLength() = 0;
};

#endif