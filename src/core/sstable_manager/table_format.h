#ifndef TABLE_FORMAT_H
#define TABLE_FORMAT_H

#include "core/sstable_manager/table_reader.h"
#include "core/iterators/iterator.h"
#include "types/entry.h"
#include "types/timestamp.h"
#include "types/types.h"

#include <memory>
#include <string>
#include <vector>

// this class exposes methods that return a `TableReader`,
// and the `TableReader` allows us to perform in-memory ops on an SSTable (
class TableFormat
{
public:
    virtual ~TableFormat() = default;

    virtual std::shared_ptr<const TableReader> openTable(const std::string &fullPath) const = 0;

    virtual std::shared_ptr<const TableReader> writeTable(
        const std::string &fullPath,
        tinykv::Iterator &entries,
        TimestampType timestamp,
        FileNumber fileNum) const = 0;
};

#endif