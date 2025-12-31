#ifndef LEVEL_MANAGER_IMPL
#define LEVEL_MANAGER_IMPL

#include <vector>
#include "core/sstable_manager/sstable_manager.h"
#include "types/error.h"
#include "types/entry.h"
#include "types/status.h"
#include "core/level_manager/level_manager.h"
#include "core/sstable_manager/sstable.h"

// contexts
#include "../../contexts/system_context.h"

class LevelManagerImpl : public LevelManager
{

public:
    LevelManagerImpl(int levelNum, std::string directoryPath, SystemContext &systemContext); // should be tied to an existing level directory. TODO: throw error if the directory doesn't exist before this is called
    const int &getLevel() override;
    std::optional<Error> writeFile(const std::vector<const Entry *> &entries) override;
    std::optional<Entry> searchKey(const std::string &key) override;
    std::pair<const_iterator, const_iterator> getFiles() override;
    std::optional<Error> deleteFiles(std::vector<const SSTableManager *> files) override;

    // reimplemented API
    std::optional<Entry> getKey(const std::string &key) override;
    Status createTable(std::vector<Entry> &&entries) override;
    std::span<const SSTable *const> getTables() override;
    Status deleteTables(std::span<const SSTable *>) override; // delete based on tableID for this particular level. We can expose SSTable.getId

    std::optional<Error> init() override;
    Status initNew() override;

private:
    int m_levelNum;
    std::string m_directoryPath; // eg. "./sstables/level-0"
    std::mutex m_mutex;
    std::vector<std::unique_ptr<SSTableManager>> m_ssTableManagers; // brute force, these guys represent files on level 0 for now
    SystemContext &m_systemContext;

    std::vector<std::unique_ptr<SSTable>> m_ssTables;

    // Helper function to generate an SSTable file name.
    std::string _generateSSTableFileName() const;
    // Helper function to get current time
    TimestampType _getTimeNow();
};

#endif