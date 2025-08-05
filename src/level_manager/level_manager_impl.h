#ifndef LEVEL_MANAGER_IMPL
#define LEVEL_MANAGER_IMPL

#include <vector>
#include "../sstable_file_manager/sstable_file_manager.h"
#include "../types/error.h"
#include "../types/entry.h"
#include "level_manager.h"

class LevelManagerImpl : public LevelManager {
    int m_levelNum;
    std::string m_directoryPath; // eg. "./sstables/level-0"
    std::mutex m_mutex;
    std::vector<std::unique_ptr<SSTableFileManager>> m_ssTableFileManagers; // brute force, these guys represent files on level 0 for now

    public:
        LevelManagerImpl(int levelNum, std::string directoryPath);
        const int & getLevel() override;
        // writes a new file
        std::optional<Error> writeFile(std::vector<const Entry*> entries) override;
        std::optional<Entry> searchKey(const std::string &key) override;
        std::optional<const SSTableFileManager*> getFiles() override;
        std::optional<Error> deleteFiles(const SSTableFileManager*) override;
};

#endif