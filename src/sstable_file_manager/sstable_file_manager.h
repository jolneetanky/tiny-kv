#ifndef SSTABLE_FILE_MANAGER_H
#define SSTABLE_FILE_MANAGER_H

#include <string>
#include <vector>
#include "../types/entry.h"
#include "../types/error.h"
#include "../types/sstable_file.h"

class SSTableFileManager {
    public:
        virtual std::optional<Error> write(std::vector<const Entry*> entryPts) = 0;
        virtual std::optional<Entry> get(const std::string& key) = 0; // searches for a key

        virtual std::optional<TimestampType> getTimestamp() = 0;
        // NOTE: this returns entries in sorted order!!
        virtual std::optional<std::vector<Entry>> getEntries() = 0;
        virtual std::string getFullPath() const = 0;
        virtual std::optional<std::string> getStartKey() const = 0;
        virtual std::optional<std::string> getEndKey() const = 0;

        virtual bool contains(std::string key) = 0; 

        virtual std::optional<Error> init() = 0;
        virtual ~SSTableFileManager() = default;
};

#endif