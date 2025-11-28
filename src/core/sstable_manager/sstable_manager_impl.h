#ifndef SSTABLE_MANAGER_IMPL_H
#define SSTABLE_MANAGER_IMPL_H

#include "types/error.h"
#include <vector>
#include "types/entry.h"
#include "types/sstable_file.h"
#include "core/sstable_manager/sstable_manager.h"
#include "core/sstable_file_manager/sstable_file_manager.h"
#include "core/level_manager/level_manager.h"

// contexts
#include "contexts/system_context.h"

// actly this can be more like LevelManager
class SSTableManagerImpl : public SSTableManager
{
private:
    int MAX_LEVEL = 2; // hardcoded for now
    std::string m_basePath = "./sstables";

    // System context
    SystemContext &m_systemContext;

    std::vector<std::unique_ptr<LevelManager>> m_levelManagers;

    std::optional<Error> _compactLevel0();
    std::optional<Error> _compactLevelN(int n);

    std::optional<std::vector<Entry>> _mergeEntries(std::vector<const Entry *> entries) const;
    std::vector<std::vector<SSTableFileManager *>> groupL0Overlaps(std::vector<SSTableFileManager *> fileManagers) const;
    std::optional<std::vector<SSTableFileManager *>> _getOverlappingFiles(int level, std::string start, std::string end) const;

public:
    SSTableManagerImpl(SystemContext &systemContext, std::string basePath = "./sstables");
    std::optional<Error> write(const std::vector<const Entry *> &entries, int level) override;

    std::optional<Entry> get(const std::string &key) const override;

    std::optional<Error> compact() override;

    std::optional<Error> initLevels(); // initializes the level managers based on existing folders on disk. Creates level 0 file manager if there's nothing
};

#endif
