#ifndef MEM_TABLE_H
#define MEM_TABLE_H

#include <iostream>
#include <optional>
#include "../disk_manager/disk_manager.h"
#include "../types/error.h"
#include "../types/entry.h"

// internal storage of MemTable is an `Entry`.
class MemTable
{
private:
    int m_size;
    // DiskManager &m_diskManager;

    // Flushes data from buffer to disk
    // Helper function to serialize data in buffer before flushing to disk
    // std::string serializeData();
    // std::string serializeData(const std::string &key, const std::string &val);

    // we can use a skip list for this.
    // SkipList skiplist;

public:
    virtual Error* put(const std::string &key, const std::string &val) = 0;
    virtual const Entry* get(const std::string &key) const = 0;
    virtual void del(const std::string &key) = 0;
    virtual bool isReadOnly() = 0;
    virtual void flushToDisk() = 0;
    virtual ~MemTable() = default;
};

#endif