// #ifndef DISK_MANAGER_IMPL_H
// #define DISK_MANAGER_IMPL_H

// #include "types/error.h"
// #include "types/status.h"
// #include <vector>
// #include "types/entry.h"
// #include "types/sstable_file.h"
// #include "core/disk_manager/disk_manager.h"
// #include "core/sstable_manager/sstable_manager.h"
// #include "core/level_manager/level_manager.h"

// // contexts
// #include "contexts/system_context.h"

// // actly this can be more like LevelManager
// class DiskManagerImpl : public DiskManager
// {
// public:
//     DiskManagerImpl(SystemContext &systemContext, std::string basePath = "./sstables");
//     std::optional<Error> write(const std::vector<const Entry *> &entries, int level) override;

//     std::optional<Entry> get(const std::string &key) const override;

//     std::optional<Error> compact() override;

//     std::optional<Error> initLevels(); // initializes the level managers based on existing folders on disk. Creates level 0 file manager if there's nothing

// private:
//     int MAX_LEVEL = 2; // hardcoded for now
//     std::string m_basePath = "./sstables";

//     // System context
//     SystemContext &m_systemContext;

//     std::vector<std::unique_ptr<LevelManager>> m_levelManagers;

//     std::optional<Error> _compactLevel0();
//     std::optional<Error> _compactLevelN(int n);

//     std::optional<std::vector<Entry>> _mergeEntries(std::vector<const Entry *> entries) const;
//     std::vector<std::vector<SSTableManager *>> groupL0Overlaps(std::vector<SSTableManager *> fileManagers) const;
//     std::optional<std::vector<SSTableManager *>> _getOverlappingFiles(int level, std::string start, std::string end) const;

//     // NEW API TO REPLACE WITH!

//     // This function looks at a level, and merges overlapping tables into one table.
//     // TODO: implement this
//     // Status _mergeOverlappingTables(int lvl);
//     // // Assumption: the files in level `n` are not overlapping. We merge this file into the lower levels.
//     // // TODO: implement this
//     // Status _compactLN(int n);

//     // // TODO: implement this
//     // std::vector<std::vector<const SSTable *>> _groupOverlappingTables(std::span<const SSTable *const> &ssTables) const;

//     Status _createDirectoryIfNotExists(std::string dirPath);
// };

// #endif