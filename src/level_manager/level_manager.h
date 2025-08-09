#ifndef LEVEL_MANAGER
#define LEVEL_MANAGER

#include <vector>
#include "../sstable_file_manager/sstable_file_manager.h"
#include "../types/error.h"
#include "../types/entry.h"

class LevelManager {

    public:
        virtual const int & getLevel() = 0;
        virtual std::optional<Error> writeFile(std::vector<const Entry*> entries) = 0;
        virtual std::optional<Entry> searchKey(const std::string &key) = 0;
        virtual std::optional<const SSTableFileManager*> getFiles() = 0;
        virtual std::optional<Error> deleteFiles(const SSTableFileManager*) = 0;
        virtual ~LevelManager() = default;
};

#endif