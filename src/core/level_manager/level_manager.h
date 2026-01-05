#ifndef LEVEL_MANAGER
#define LEVEL_MANAGER

#include "core/sstable_manager/sstable_manager.h"
#include "core/sstable_manager/sstable.h"
#include "types/error.h"
#include "types/entry.h"
#include "types/status.h"
#include <span>

/*
RESPONSIBILITIES:
1. Owns the SSTables in this level.
2. Creates SSTables given a list of entries.
3. Searches for a key within this level.
4. Handles compaction with another level.

INVARIANTS:
1. The LevelManager is not responsible for ensuring the directory it should exist in has been created. It is the responsibility of anyone creating the LevelManager to ensure that associated directory exists.
2. Returned SSTable pointers are only valid until the next compaction or mutation of the level.
3. The LevelManager may or may not be empty.
*/
class LevelManager
{

public:
    using const_iterator = std::vector<std::unique_ptr<SSTableManager>>::const_iterator;

    virtual const int &getLevel() = 0;

    virtual std::optional<Entry> getKey(const std::string &key) const = 0;

    virtual Status createTable(std::vector<Entry> &&entries) = 0;

    // This function compacts all SSTables in this level into `other`.
    // Entries in this level with the same key will override the same entries in `other`, hence compacting by reducing the number of files.
    virtual Status compactInto(LevelManager &other) = 0;

    // Initializes the level with SSTables for each file in this level.
    // ASSUMPTION: the related directory has already been created.
    virtual Status initNew() = 0;

    virtual std::optional<Error> init() = 0;
    virtual ~LevelManager() = default;
};

#endif