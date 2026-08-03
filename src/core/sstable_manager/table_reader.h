#ifndef TABLE_READER_H
#define TABLE_READER_H

#include "core/iterators/iterator.h"
#include "core/sstable_manager/sstable_metadata.h"
#include "types/entry.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

class TableReader
{
public:
    virtual ~TableReader() = default;

    // TODO: Return this table's metadata without loading all entries.
    virtual SSTableMetadata meta() const = 0;

    // TODO: Return true if key is within this table's inclusive key range.
    virtual bool withinRange(const std::string &key) const = 0;

    // TODO: Perform an optimized point lookup for key.
    virtual std::optional<Entry> get(const std::string &key) const = 0;

    // TODO: Return an iterator for scans/compaction.
    virtual std::unique_ptr<tinykv::Iterator> NewIterator() const = 0;

    // TODO: Return logical entry count if known.
    virtual std::size_t getSize() const = 0;
};

#endif
