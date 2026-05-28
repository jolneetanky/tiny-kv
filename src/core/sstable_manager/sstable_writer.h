#ifndef SSTABLE_WRITER_H
#define SSTABLE_WRITER_H
#include "core/sstable_manager/sstable_metadata.h"
#include "types/entry.h"
#include "types/timestamp.h"
#include "types/types.h"

/*
This class is responsible for writing entries to disk as an SSTable.
It returns the metadata associated with the file.
Essentially: given a path and some entries, materialize bytes in the SSTable format.

It is a functional class, meaning it's stateless.
*/
class SSTableWriter
{
public:
    /*
    This method writes entries to disk as an SSTable.
    Side effect: After this, it is guaranteed that the SSTable file is actually written to disk. Specifically, it is written to `full_path`.
    It will overwrite any existing file at `full_path`.
    For safety, the caller should be sure that no existing SSTable already exists at `full_path` before calling `SSTableWriter::write(full_path, ...)`
    Returns `SSTableMetadata`.
    */
    SSTableMetadata write(const std::string &full_path, std::vector<Entry> &entries, TimestampType timestamp, FileNumber file_num);

private:
    std::string _generateSSTableFileName() const;
    bool _writeBinaryToFile(const std::string &path, const std::string &data);
    bool _createFileIfNotExists(const std::string &fullPath) const;
    std::string _serializeEntry(const Entry &entry) const;
    TimestampType _getTimeNow() const;
};

#endif