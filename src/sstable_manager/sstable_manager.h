#ifndef SSTABLE_MANAGER_H
#define SSTABLE_MANAGER_H

#include "../types/error.h"
#include <vector>
#include "../types/entry.h"

class SSTableManager {
    public:
        virtual Error* write(std::vector<const Entry*> entries) = 0;
        virtual std::vector<const Entry*> read(std::string file) = 0;
        virtual ~SSTableManager() = default;
};

#endif