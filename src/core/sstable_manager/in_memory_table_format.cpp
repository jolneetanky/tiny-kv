#include "core/sstable_manager/in_memory_table_format.h"

#include "core/sstable_manager/in_memory_table_reader.h"
#include "core/sstable_manager/sstable.h"
#include "core/sstable_manager/sstable_reader.h"
#include "core/sstable_manager/sstable_writer.h"

std::shared_ptr<const TableReader> InMemoryTableFormat::openTable(const std::string &fullPath) const
{
    SSTableReader reader;
    auto table = std::make_unique<SSTable>(reader.read(fullPath));
    return std::make_shared<InMemoryTableReader>(std::move(table));
}

std::shared_ptr<const TableReader> InMemoryTableFormat::writeTable(
    const std::string &fullPath,
    tinykv::Iterator &entries,
    TimestampType timestamp,
    FileNumber fileNum) const
{
    std::vector<Entry> materializedEntries;
    for (entries.SeekToFirst(); entries.Valid(); entries.Next())
    {
        materializedEntries.emplace_back(entries.Key(), entries.Value(), entries.isTombstone());
    }

    SSTableWriter writer;
    SSTableMetadata metadata = writer.write(fullPath, materializedEntries, timestamp, fileNum);
    auto table = std::make_unique<SSTable>(metadata, std::move(materializedEntries));
    return std::make_shared<InMemoryTableReader>(std::move(table));
}
