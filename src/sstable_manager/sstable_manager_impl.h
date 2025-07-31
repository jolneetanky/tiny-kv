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

        std::string LEVEL_0_DIRECTORY { "./sstables/level-0/"};

        // unique_ptr => ensures there is exactly one owner of the object at any time
        // destroyed once the object goes out of scope
        
        // this acts as an in-memroy index for us to access a certain file. I will refine later
        // this is how it looks like in a level
        std::vector<std::unique_ptr<SSTableFileManager>> m_ssTableFileManagers; // brute force, these guys represent files on level 0 for now

        std::vector<SSTableFileManager*> getFilesFromLevel(int level);

    public:
        std::optional<Error> write(std::vector<const Entry*> entries, int level) override;

        std::optional<Entry> get(const std::string& key) override;

        std::optional<Error> initLevels(); 
};

#endif