#ifndef LEVEL_MANAGER_IMPL
#define LEVEL_MANAGER_IMPL

#include <vector>
#include "types/error.h"
#include "types/entry.h"
#include "types/status.h"
#include "core/level_manager/level_manager.h"
#include "core/sstable_manager/sstable.h"

// contexts
#include "../../contexts/system_context.h"

/*
INVARIANTS (implementation detail that only affects LevelManager but not its API usage. Could be changed depending on how we implement LevelManager.):
1. m_ssTables always maintains chronological order (newest entry inserted in front), so we know which entry should override which.
We maintain chronological order when inserting a new SSTable (which doesn't happen often, only during flush / writes. But we assume GETs happen much more frequent than WRITEs so we optimize for more GETs instead.)
2. Only if `level == 0`, then we allow overlaps.
3. A LevelManager contains overlapping tables if `m_allowOverlap` == true. Overlaps are enforced during insertion depending on this rule.
4. After `compactInto()`, `this` becomes an empty level, and `other` is NOT overlapping.
*/
class LevelManagerImpl : public LevelManager
{

public:
    LevelManagerImpl(int levelNum, std::string directoryPath, SystemContext &systemContext); // should be tied to an existing level directory. TODO: throw error if the directory doesn't exist before this is called
    const int &getLevel() override;

    // reimplemented API
    std::optional<Entry> getKey(const std::string &key) const override;
    Status createTable(std::vector<Entry> &&entries) override;
    Status compactInto(LevelManager &other) override;

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
    std::mutex m_mutex;
    SystemContext &m_systemContext;
    bool m_allowOverlap;

    std::vector<std::unique_ptr<SSTable>> m_ssTables;

    // Helper function to generate an SSTable file name.
    std::string _generateSSTableFileName() const;
    // Helper function to get current time
    TimestampType _getTimeNow();
    Status _mergeOverlappingTables();
    Status _deleteTables(std::vector<const SSTable *> &tables);
};

#endif