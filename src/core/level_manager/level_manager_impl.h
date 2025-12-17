#ifndef LEVEL_MANAGER_IMPL
#define LEVEL_MANAGER_IMPL

#include <vector>
#include "core/sstable_manager/sstable_manager.h"
#include "types/error.h"
#include "types/entry.h"
#include "core/level_manager/level_manager.h"

// contexts
#include "../../contexts/system_context.h"

class LevelManagerImpl : public LevelManager
{
    int m_levelNum;
    std::string m_directoryPath; // eg. "./sstables/level-0"
    std::mutex m_mutex;
    std::vector<std::unique_ptr<SSTableManager>> m_ssTableManagers; // brute force, these guys represent files on level 0 for now
    SystemContext &m_systemContext;

public:
    LevelManagerImpl(int levelNum, std::string directoryPath, SystemContext &systemContext); // should be tied to an existing level directory. TODO: throw error if the directory doesn't exist before this is called
    const int &getLevel() override;
    std::optional<Error> writeFile(const std::vector<const Entry *> &entries) override;
    std::optional<Entry> searchKey(const std::string &key) override;
    std::pair<const_iterator, const_iterator> getFiles() override;
    std::optional<Error> deleteFiles(std::vector<const SSTableManager *> files) override;
    // initialize this class with SSTableFileManagers
    std::optional<Error> init() override;
};

#endif