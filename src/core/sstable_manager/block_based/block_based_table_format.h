#ifndef BLOCK_BASED_TABLE_FORMAT_H
#define BLOCK_BASED_TABLE_FORMAT_H

#include "core/sstable_manager/table_format.h"

class BlockBasedTableFormat final : public TableFormat
{
public:
    std::shared_ptr<const TableReader> openTable(const std::string &fullPath) const override;

    // assumptions
    // 1. `entries` is alr sorted
    std::shared_ptr<const TableReader> writeTable(
        const std::string &fullPath,
        tinykv::Iterator &entries,
        TimestampType timestamp,
        FileNumber fileNum) const override;
};

#endif