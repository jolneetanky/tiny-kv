#ifndef SSTABLE_FILE_H
#define SSTABLE_FILE_H

#include <vector>
#include "entry.h"
#include "timestamp.h"

struct SSTableFile {
    public:
        std::vector<Entry> entries;
        TimestampType timestamp;

        SSTableFile() = default;

        SSTableFile(const std::vector<Entry>& entries_, TimestampType timestamp_) : entries(entries_), timestamp(timestamp_) {};
};

#endif