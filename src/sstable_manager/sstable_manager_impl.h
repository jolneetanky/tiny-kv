#ifndef SSTABLE_MANAGER_IMPL_H
#define SSTABLE_MANAGER_IMPL_H

#include "../types/error.h"
#include <vector>
#include "../types/entry.h"
#include "../types/sstable_file.h"
#include "sstable_manager.h"
#include "../sstable_file_manager/sstable_file_manager.h"

class SSTableManagerImpl : public SSTableManager {
    private:
        std::string LEVEL_0_DIRECTORY { "./sstables/level-0/"};

        // unique_ptr => ensures there is exactly one owner of the object at any time
        // destroyed once the object goes out of scope
        std::vector<std::unique_ptr<SSTableFileManager>> m_ssTableFileManagers; // brute force, these guys represent files on level 0 for now

        std::vector<SSTableFileManager*> getFilesFromDirectory(const std::string &dirName);

        // file operations
        // write new file, read new file etc.
        // std::string serializeEntry(const Entry &entry);
        // std::optional<Entry> deserializeEntry(const char* data, size_t size, size_t& bytesRead);
        // bool writeBinaryToFile(const std::string& path, const std::string& data);
        // bool readBinaryFromFile(const std::string& filename, std::string& outData);
        // SSTableFile::TimestampType getTimestamp();
        // std::optional<SSTableFile> read(std::string file);

    public:
        // writes a new SSTable file, for now to level 0. No need to think about directories for now
        std::optional<Error> write(std::vector<const Entry*> entries) override;

        // Looks for a key on disk and returns the corresponding Entry (if any).
        std::optional<Entry> get(const std::string& key) override;
};

#endif