#ifndef SSTABLE_META_H
#define SSTABLE_META_H

#include <stdint.h>
#include "types/timestamp.h"
#include <string>

struct SSTableMetadata
{
    uint64_t file_number;
    TimestampType timestamp;
    std::string min_key, max_key;

    // ordering rules for compaction
    bool operator<(const SSTableMetadata &other) const
    {
        if (this->file_number != other.file_number)
        {
            return this->file_number < other.file_number;
        }

        // fallback logic (shouldn't happen though, as file_number should be unique)
        return this->timestamp < other.timestamp;
    }
};

#endif