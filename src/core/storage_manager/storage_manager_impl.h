#ifndef DISK_MANAGER_IMPL_NEW_H
#define DISK_MANAGER_IMPL_NEW_H

#include "types/error.h"
#include "types/status.h"
#include <vector>
#include "types/entry.h"
#include "types/sstable_file.h"
#include "core/storage_manager/storage_manager.h"
#include "core/level_manager/level_manager.h"
#include "core/sstable_manager/table_format.h"

// contexts
#include "contexts/system_context.h"

// actly this can be more like LevelManager
class StorageManagerImpl : public StorageManager
{
public:
    StorageManagerImpl(SystemContext &systemContext, std::string basePath, int maxLevel, std::unique_ptr<const TableFormat> tableFormat);

    std::optional<Error> write(tinykv::Iterator &entries, int level) override;

    std::optional<Entry> get(const std::string &key) const override;

    std::optional<Error> compact() override;

    // initializes the level managers based on existing folders on disk.
    // Creates all file managers up to MAX_LEVEL.
    // For each level, if the level dir exists, call `levelManagers[i].init()` to load SSTables into that level.
    // Else, create a new dir and store the LevelManager as it is.
    std::optional<Error> initLevels(); // initializes the level managers based on existing folders on disk. Creates level 0 file manager if there's nothing

private:
    int m_maxLevel;
    std::string m_basePath;

    // System context
    SystemContext &m_systemContext;

    std::unique_ptr<const TableFormat> m_tableFormat;

    std::vector<std::unique_ptr<LevelManager>> m_levelManagers;

    // TODO: implement this
    Status _compactLN(int n);
    Status _createDirectoryIfNotExists(std::string dirPath);
};

#endif
