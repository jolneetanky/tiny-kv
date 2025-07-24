#ifndef SSTABLE_MANAGER_IMPL_H
#define SSTABLE_MANAGER_IMPL_H

#include "../types/error.h"
#include <vector>
#include "../types/entry.h"
#include "sstable_manager.h"

class SSTableManagerImpl : public SSTableManager {
    private:
        std::string serializeEntry(const Entry &entry);
        Error m_writeError {Error("Failed to write SSTable")};

    public:
        Error* write(std::vector<const Entry*> entries) override;
        std::vector<const Entry*> read(std::string file) override;
};

#endif