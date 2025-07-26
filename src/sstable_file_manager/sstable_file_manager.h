#ifndef SSTABLE_FILE_MANAGER_H
#define SSTABLE_FILE_MANAGER_H

#include <string>
#include <vector>
#include "../types/entry.h"
#include "../types/error.h"

class SSTableFileManager {
    public:
        virtual std::optional<Error> write(std::vector<const Entry*> entryPts) = 0;
        virtual std::optional<Entry> get(const std::string& key) const = 0; // searches for a key
        virtual std::optional<SSTableFile::TimestampType> getTimestamp() const = 0;
        virtual ~SSTableFileManager() = default;
};

#endif