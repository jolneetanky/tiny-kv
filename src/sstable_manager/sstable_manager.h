#ifndef SSTABLE_MANAGER_H
#define SSTABLE_MANAGER_H

#include "../types/error.h"
#include <vector>
#include "../types/entry.h"

class SSTableManager {
    public:
        virtual std::optional<Error> write(std::vector<const Entry*> entries) = 0;
        virtual std::optional<Entry> get(const std::string& key) = 0;
        virtual ~SSTableManager() = default;
};

#endif