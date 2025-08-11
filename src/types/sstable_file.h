#ifndef SSTABLE_FILE_H
#define SSTABLE_FILE_H

#include <vector>
#include "entry.h"
#include "timestamp.h"

struct SSTableFile {
    public:
        // using TimestampType = long long;
        std::vector<Entry> entries; // all the key-value pairs
        TimestampType timestamp; // time at which this file was written

        SSTableFile() = default;

        SSTableFile(const std::vector<Entry>& entries_, TimestampType timestamp_) : entries(entries_), timestamp(timestamp_) {};
};

#endif