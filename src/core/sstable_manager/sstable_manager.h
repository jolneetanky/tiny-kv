#ifndef SSTABLE_MANAGER_H
#define SSTABLE_MANAGER_H

#include "types/error.h"
#include "types/entry.h"
#include <vector>

// This class is the single source of truth for files and levels.
// Hence should expose methods to update files and levels.
class SSTableManager
{
public:
    virtual std::optional<Error> write(std::vector<const Entry *> entries, int level) = 0;
    virtual std::optional<Entry> get(const std::string &key) const = 0;
    virtual std::optional<Error> compact() = 0;
    virtual ~SSTableManager() = default;
};

#endif