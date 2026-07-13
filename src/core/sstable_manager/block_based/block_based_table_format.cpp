#include "core/sstable_manager/block_based/block_based_table_format.h"

#include "core/sstable_manager/block_based/block_based_table_reader.h"
#include "core/sstable_manager/block_based/block_based_sstable_structs.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>

// #include "core/sstable_manager/sstable.h"
// #include "core/sstable_manager/sstable_reader.h"
// #include "core/sstable_manager/sstable_writer.h"

namespace
{
    void writeUint64BigEndian(std::ostream &output, std::uint64_t value)
    {
        std::array<unsigned char, sizeof(std::uint64_t)> bytes{}; // create an array of size 8 bytes (64 bits)
        for (auto it = bytes.rbegin(); it != bytes.rend(); ++it)
        {
            *it = static_cast<unsigned char>(value & 0xff); // read the lowest 8 bits (1 byte) into the `bytes` array
            value >>= 8;
        }

        // now `bytes` looks like eg. [00, 00, 00, 00, 00, 00, 01, 2c]
        output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        if (!output)
        {
            throw std::runtime_error("failed to write uint64");
        }
    }

    void writeBlockHandle(std::ostream &output, const BlockHandle &handle)
    {
        writeUint64BigEndian(output, handle.offset);
        writeUint64BigEndian(output, handle.size);
    }

    void writeFooter(std::ostream &output, const Footer &footer)
    {
        writeBlockHandle(output, footer.metadata_block);
        writeBlockHandle(output, footer.index_block);
        writeBlockHandle(output, footer.filter_block);
    }
}

std::shared_ptr<const TableReader> BlockBasedTableFormat::openTable(const std::string &fullPath) const
{
    return std::make_shared<BlockBasedTableReader>(fullPath);
}

std::shared_ptr<const TableReader> BlockBasedTableFormat::writeTable(
    const std::string &fullPath,
    tinykv::Iterator &entries,
    TimestampType timestamp,
    FileNumber fileNum) const
{
    (void)entries;
    (void)timestamp;
    (void)fileNum;

    // TODO: write data, index, metadata, and filter blocks before the footer.
    // For now this only writes an empty footer so the reader can validate the
    // fixed-width footer encoding without depending on C++ struct layout.
    const std::filesystem::path tablePath{fullPath};
    if (tablePath.has_parent_path())
    {
        std::filesystem::create_directories(tablePath.parent_path());
    }

    std::ofstream output{fullPath, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        throw std::runtime_error("BlockBasedTableFormat: failed to create table: " + fullPath);
    }

    writeFooter(output, Footer{});

    // 2. create table reader and return it
    return std::make_shared<BlockBasedTableReader>(fullPath);

    // std::vector<Entry> materializedEntries;
    // for (entries.SeekToFirst(); entries.Valid(); entries.Next())
    // {
    //     materializedEntries.emplace_back(entries.Key(), entries.Value(), entries.isTombstone());
    // }

    // SSTableWriter writer;
    // SSTableMetadata metadata = writer.write(fullPath, materializedEntries, timestamp, fileNum);
    // auto table = std::make_unique<SSTable>(metadata, std::move(materializedEntries));
    // return std::make_shared<InMemoryTableReader>(std::move(table));
}
