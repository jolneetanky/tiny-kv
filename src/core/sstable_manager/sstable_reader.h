#ifndef SSTABLE_READER_H
#define SSTABLE_READER_H

#include "core/sstable_manager/sstable.h"
#include "types/entry.h"
#include "types/sstable_file.h"

/*
SSTable DISK LAYOUT:

| Entry 0 | Entry 1 | ... | Entry N |
| timestamp (sizeof TimestampType) |
| file_num (sizeof FileNumber) |
*/

/*
Utility class that reads an SSTable on disk, parsing it into an SSTable and returns this SSTable instance.
*/
class SSTableReader
{
public:
    /*
    Reads a file and parses it into an SSTable, then returns that SSTable.
    */
    SSTable read(const std::string &full_path);

private:
    Entry _deserializeEntry(const char *data, size_t size, size_t &bytesRead) const;
    bool _readBinaryFromFile(const std::string &filename, std::string &outData) const;
    // TimestampType _getTimeNow(); // helper to get current timestamp
    // SSTableFile _decode(std::string file) const;

    // std::optional<Error> _readFileToMemory(); // Reads file, and stores it in-memory within this object.
    // std::optional<Error> _init();             // Reads file to memory, and initializes Bloom Filter
};

#endif