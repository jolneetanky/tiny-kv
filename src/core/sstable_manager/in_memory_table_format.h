#ifndef IN_MEMORY_TABLE_FORMAT_H
#define IN_MEMORY_TABLE_FORMAT_H

#include "core/sstable_manager/table_format.h"

class InMemoryTableFormat final : public TableFormat
{
public:
    std::shared_ptr<const TableReader> openTable(const std::string &fullPath) const override;

    std::shared_ptr<const TableReader> writeTable(
        const std::string &fullPath,
        tinykv::Iterator &entries,
        TimestampType timestamp,
        FileNumber fileNum) const override;
};

#endif
