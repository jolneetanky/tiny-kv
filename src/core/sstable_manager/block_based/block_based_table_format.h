#ifndef BLOCK_BASED_TABLE_FORMAT_H
#define BLOCK_BASED_TABLE_FORMAT_H

#include "core/sstable_manager/table_format.h"

class BlockBasedTableFormat final : public TableFormat
{
public:
    std::shared_ptr<const TableReader> openTable(const std::string &fullPath) const override;

    // REQUIREMENTS
    // 1. `entries` must be sorted by key in nondecreasing order.
    // This is because the writer writes the entires in the order that they came.
    // and TableReader requires sorted order so that it can perform binary search on the index block
    // and so that within each data block, it can perform binary search to find the key.

    // 2. `entries` must contain no duplicate keys.
    // This is to maintain the invariant that an SStalbe file has no duplicate keys.

    // The writer keeps the order within each data block and writes one index
    // entry per non-empty block, using that block's final key as `last_key`.
    std::shared_ptr<const TableReader> writeTable(
        const std::string &fullPath,
        tinykv::Iterator &entries,
        TimestampType timestamp,
        FileNumber fileNum) const override;
};

#endif
