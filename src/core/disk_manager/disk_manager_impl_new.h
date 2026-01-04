#ifndef DISK_MANAGER_IMPL_NEW_H
#define DISK_MANAGER_IMPL_NEW_H

#include "types/error.h"
#include "types/status.h"
#include <vector>
#include "types/entry.h"
#include "types/sstable_file.h"
#include "core/disk_manager/disk_manager.h"
#include "core/sstable_manager/sstable_manager.h"
#include "core/level_manager/level_manager.h"

// contexts
#include "contexts/system_context.h"

// actly this can be more like LevelManager
class DiskManagerImpl : public DiskManager
{
public:
    DiskManagerImpl(SystemContext &systemContext, std::string basePath = "./sstables");
    std::optional<Error> write(const std::vector<const Entry *> &entries, int level) override;

    std::optional<Entry> get(const std::string &key) const override;

    std::optional<Error> compact() override;

    // initializes the level managers based on existing folders on disk. Creates all file managers up to MAX_LEVEL if there's nothing
    std::optional<Error> initLevels(); // initializes the level managers based on existing folders on disk. Creates level 0 file manager if there's nothing

private:
    int MAX_LEVEL = 2; // hardcoded for now
    std::string m_basePath = "./sstables";

    // System context
    SystemContext &m_systemContext;

    std::vector<std::unique_ptr<LevelManager>> m_levelManagers;

    // TODO: implement this
    Status _compactLN(int n);
    Status _createDirectoryIfNotExists(std::string dirPath);
};

#endif
