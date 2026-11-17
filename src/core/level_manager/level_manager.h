#ifndef LEVEL_MANAGER
#define LEVEL_MANAGER

#include "core/sstable_manager/sstable.h"
#include "core/level_manager/compaction_planner.h"
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
    virtual const int &getLevel() = 0;

    virtual size_t tableCount() const = 0;

    // Returns metadata in the same identity space used by CompactionPlan
    // (file numbers identify tables; vector order is not significant).
    virtual std::vector<SSTableMetadata> tableMetadata() const = 0;

    virtual std::optional<Entry> getKey(const std::string &key) const = 0;

    virtual Status createTable(tinykv::Iterator &entries) = 0;

    // Executes one planned overlap group, writing its replacement output into
    // `destination`. The source level is newer when the same key exists in both.
    virtual Status compactInto(LevelManager &destination, const tinykv::CompactionPlan &plan) = 0;

    // Initializes the level with SSTables for each file in this level.
    // ASSUMPTION: the related directory has already been created.
    // Each SSTable file should be opened once by its owning LevelManager.
    // That LevelManager owns the shared_ptr for that table.
    // Other code shares that pointer only through the LevelManager.
    virtual Status initNew() = 0;

    virtual ~LevelManager() = default;
};

#endif
