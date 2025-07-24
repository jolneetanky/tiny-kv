#ifndef SSTABLE_MANAGER_IMPL_H
#define SSTABLE_MANAGER_IMPL_H

#include "../types/error.h"
#include <vector>
#include "../types/entry.h"
#include "sstable_manager.h"

class SSTableManagerImpl : public SSTableManager {
    private:
        std::string serializeEntry(const Entry &entry);
        Entry deserializeEntry(const char* data, size_t size, size_t& bytesRead);
        bool writeBinaryToFile(const std::string& path, const std::string& data);
        bool readBinaryFromFile(const std::string& filename, std::string& outData);

    public:
        std::optional<Error> write(std::vector<const Entry*> entries) override;
        std::optional<std::vector<Entry>> read(std::string file) override;
        // Looks for a key on disk and returns a pointer to the corresponding Entry (if any).
        std::optional<Entry> find(const std::string& key) override;
};

#endif