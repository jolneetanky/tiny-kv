#ifndef SSTABLE_MANAGER_IMPL_H
#define SSTABLE_MANAGER_IMPL_H

#include "../types/error.h"
#include <vector>
#include "../types/entry.h"
#include "../types/sstable_file.h"
#include "sstable_manager.h"
#include "../sstable_file_manager/sstable_file_manager.h"

#include "../level_manager/level_manager.h"

// actly this can be more like LevelManager
class SSTableManagerImpl : public SSTableManager {
    private:
        int MAX_LEVEL = 2; // hardcoded for now        
        std::string BASE_PATH = "./sstables";

        std::vector<std::unique_ptr<LevelManager>> m_levelManagers;

        std::optional<Error> _compactLevel0();
        std::optional<Error> _compactLevelN(int n);

        std::optional<std::vector<Entry>> _mergeEntries(std::vector<const Entry*> entries) const;
        std::vector<std::vector<SSTableFileManager*>> groupL0Overlaps(std::vector<SSTableFileManager*> fileManagers) const;
        std::optional<std::vector<SSTableFileManager*>> _getOverlappingFiles(int level, std::string start, std::string end) const;

    public:
        std::optional<Error> write(std::vector<const Entry*> entries, int level) override;

        std::optional<Entry> get(const std::string& key) const override;

        std::optional<Error> compact() override;

        std::optional<Error> initLevels(); // initializes the level managers based on existing folders on disk. Creates level 0 file manager if there's nothing
};

#endif