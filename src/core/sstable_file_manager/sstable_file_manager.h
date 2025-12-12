#ifndef SSTABLE_FILE_MANAGER_H
#define SSTABLE_FILE_MANAGER_H

#include <string>
#include <vector>
#include "types/entry.h"
#include "types/error.h"
#include "types/sstable_file.h"
#include "core/sstable_file_manager/sstable_metadata.h"
/*
INVARIANTS:
1. an SSTableFileManager MUST contain entries. It must also correspond to a file that already exists.
- if a new SSTableFileManager is to be created that doesn't yet exist, we must pass it a vector of entries to populate the SSTableFile with.
- in the ctor, the SSTableFileManager will write these entries to a file, thereby representing a file that exists on disk + which is populated by entries.
- no write. Only 2 ctors:
    1. SSTableFileManagerImpl::SSTableFileManagerImpl(const std::string &directoryPath, const std::string &fileName, SystemContext &systemCtx) for an already existing SSTable on disk
    2. `SSTableFileManagerImpl::SSTableFileManagerImpl(std::string directoryPath, SystemContext &systemCtx, vector<Entry*> entries) -- creates a new file with these entries.
2. `fileNumber` corresponds exactly to order of creation. Larger file number -> newer file.
*/

// can just directly have a comparator that the compactor can call.
// PROS: if we wanna change the ordering of SSTableFiles, only need to change it here, and comparator stays the same.

class SSTableFileManager
{
protected:
    SSTableMetadata m_metadata; // should only be accessed by child classes

public:
    virtual std::optional<Entry> get(const std::string &key) = 0;

    virtual uint64_t getFileNumber() = 0; // not optional, because every file manager MUST have a file number.
    virtual std::optional<TimestampType> getTimestamp() = 0;
    virtual std::optional<std::vector<Entry>> getEntries() = 0; // returns entries in sorted order
    virtual std::string getFullPath() const = 0;
    virtual std::optional<std::string> getStartKey() = 0;
    virtual std::optional<std::string> getEndKey() = 0;

    virtual bool contains(std::string key) = 0;

    const SSTableMetadata &meta() const
    {
        return m_metadata;
    }

    virtual ~SSTableFileManager() = default;
};

#endif