#ifndef SSTABLE_FILE_MANAGER_H
#define SSTABLE_FILE_MANAGER_H

#include <string>
#include <vector>
#include "types/entry.h"
#include "types/error.h"
#include "types/sstable_file.h"

/*
SEMANTICS:
- an SSTableFileManager must contain entries. It must also correspond to a file that already exists.
- if a new SSTableFileManager is to be created that doesn't yet exist, we must pass it a vector of entries to populate the SSTableFile with.
- in the ctor, the SSTableFileManager will write these entries to a file, thereby representing a file that exists on disk + which is populated by entries.
- no write. Only 2 ctors:
1. SSTableFileManagerImpl::SSTableFileManagerImpl(const std::string &directoryPath, const std::string &fileName, SystemContext &systemCtx) for an already existing SSTable on disk
2. `SSTableFileManagerImpl::SSTableFileManagerImpl(std::string directoryPath, SystemContext &systemCtx, vector<Entry*> entries) -- creates a new file with these entries.
*/
class SSTableFileManager
{
public:
    virtual std::optional<Entry> get(const std::string &key) = 0;

    virtual std::optional<TimestampType> getTimestamp() = 0;
    virtual std::optional<std::vector<Entry>> getEntries() = 0; // returns entries in sorted order
    virtual std::string getFullPath() const = 0;
    virtual std::optional<std::string> getStartKey() = 0;
    virtual std::optional<std::string> getEndKey() = 0;

    virtual bool contains(std::string key) = 0;

    virtual ~SSTableFileManager() = default;
};

#endif