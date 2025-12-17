#ifndef LEVEL_MANAGER
#define LEVEL_MANAGER

#include <vector>
#include "core/sstable_manager/sstable_manager.h"
#include "types/error.h"
#include "types/entry.h"

class LevelManager
{

public:
    using const_iterator = std::vector<std::unique_ptr<SSTableManager>>::const_iterator;

    virtual const int &getLevel() = 0;
    virtual std::optional<Error> writeFile(const std::vector<const Entry *> &entries) = 0;
    virtual std::optional<Entry> searchKey(const std::string &key) = 0;
    virtual std::pair<const_iterator, const_iterator> getFiles() = 0;
    virtual std::optional<Error> deleteFiles(std::vector<const SSTableManager *> files) = 0;
    virtual std::optional<Error> init() = 0;
    virtual ~LevelManager() = default;
};

#endif