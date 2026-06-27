#ifndef LEVEL_MANAGER_IMPL
#define LEVEL_MANAGER_IMPL

#include "types/error.h"
#include "types/entry.h"
#include "types/status.h"
#include "core/level_manager/level_manager.h"
#include "core/sstable_manager/sstable.h"
#include "core/bloom_filter/bloom_filter_impl.h"

// contexts
#include "../../contexts/system_context.h"

// std lib
#include <vector>
#include <algorithm>
#include <memory>

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
    // TableHandle struct containing everything needed to represent and operate on an SSTable
    // TODO: refactor to block-based design.
    // Instead of storing in-memory SSTables, it will just load
    // 1. metadata block
    // 2. index block
    // 3. bloom filter blocks
    struct TableHandle
    {
        static constexpr size_t kBloomBitsPerEntry = 10; // `static` so this is shared by all TableHandles
        static constexpr size_t kBloomHashCount = 7;

        std::unique_ptr<SSTable> table;
        BloomFilterImpl bloom;

        // just pass in the table, and weconstruct the bloom filter
        TableHandle(std::unique_ptr<SSTable> table_) : table(std::move(table_)), bloom(std::max<std::size_t>(1, table->getSize() * kBloomBitsPerEntry), kBloomHashCount)
        {
            for (const Entry &entry : table->getEntries())
            {
                bloom.insert(entry.key);
            }
        }
    };

    std::string m_logPrefix;
    int m_levelNum;
    std::string m_directoryPath; // eg. "./sstables/level-0"
    std::mutex m_mutex;
    SystemContext &m_systemContext;
    bool m_allowOverlap;

    // std::vector<std::unique_ptr<SSTable>> m_ssTables;
    std::vector<TableHandle> m_tableHandles; // rn, it's fine to just store `TableHandle` because we don't need to support polymorphism of TableHandle yet.

    // Helper function to generate an SSTable file name.
    std::string _generateSSTableFileName() const;
    // Helper function to get current time
    TimestampType _getTimeNow();
    Status _mergeOverlappingTables();
    Status _deleteTables(std::vector<const SSTable *> &tables);
};

#endif
