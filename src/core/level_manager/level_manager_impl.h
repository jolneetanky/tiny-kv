#ifndef LEVEL_MANAGER_IMPL
#define LEVEL_MANAGER_IMPL

#include "types/error.h"
#include "types/entry.h"
#include "types/status.h"
#include "core/level_manager/level_manager.h"
#include "core/sstable_manager/sstable.h"
#include "core/sstable_manager/table_format.h"
#include "core/sstable_manager/table_reader.h"

// contexts
#include "../../contexts/system_context.h"

// std lib
#include <vector>
#include <algorithm>
#include <memory>

/*
INVARIANTS (implementation detail that only affects LevelManager but not its API usage. Could be changed depending on how we implement LevelManager.):
1. m_tableReaders always maintains chronological order (oldest first, newest last), so reverse iteration checks newer tables first.
We append new SSTables in O(1) and search from newest to oldest for lookups.
2. Only if `level == 0`, then we allow overlaps.
3. A LevelManager contains overlapping tables if `m_allowOverlap` == true. Overlaps are enforced during insertion depending on this rule.
4. After `compactInto()`, `this` becomes an empty level, and `other` is NOT overlapping.
*/
class LevelManagerImpl : public LevelManager
{

public:
    LevelManagerImpl(int levelNum, std::string directoryPath, SystemContext &systemContext, const TableFormat &tableFormat); // should be tied to an existing level directory. TODO: throw error if the directory doesn't exist before this is called
    const int &getLevel() override;
    size_t tableCount() const override;
    std::vector<SSTableMetadata> tableMetadata() const override;

    // reimplemented API
    std::optional<Entry> getKey(const std::string &key) const override;
    Status createTable(tinykv::Iterator &entries) override;
    Status compactInto(LevelManager &destination, const tinykv::CompactionPlan &plan) override;

    // Reads the files in this directory, and loads them into memory as an SSTable.
    // Who calls this? For now, anyone can call this.
    // TODO: maybe we should hide this as it exposes internal state.
    // ASSUMPTIONS:
    // 1. Corresponding directory already exists.
    Status initNew() override;

private:
    std::string m_logPrefix;
    int m_levelNum;
    std::string m_directoryPath; // eg. "./sstables/level-0"
    SystemContext &m_systemContext;
    const TableFormat &m_tableFormat;
    bool m_allowOverlap;

    // Kept oldest-first by durable file number. Readers expose their source paths
    // so compaction can remove input files after writing replacements.
    std::vector<std::shared_ptr<const TableReader>> m_tableReaders;

    // Helper function to generate an SSTable file name.
    std::string _generateSSTableFileName() const;
    // Helper function to get current time
    TimestampType _getTimeNow();
    // TODO: Rework compaction against TableReader iterators.
    // Status _mergeOverlappingTables();
    // Status _deleteTables(std::vector<const SSTable *> &tables);
};

#endif
