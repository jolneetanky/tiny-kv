#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include "types/error.h"
#include "types/entry.h"
#include "types/status.h"
#include <vector>

// This class is the single source of truth for files and levels.
// Hence should expose methods to update files and levels.
class StorageManager
{
public:
    virtual std::optional<Error> write(const std::vector<const Entry *> &entries, int level) = 0;
    virtual std::optional<Entry> get(const std::string &key) const = 0;
    virtual std::optional<Error> compact() = 0;
    virtual ~StorageManager() = default;
};

#endif