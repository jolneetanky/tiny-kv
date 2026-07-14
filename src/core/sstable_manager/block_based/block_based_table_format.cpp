#include "core/sstable_manager/block_based/block_based_table_format.h"

#include "core/sstable_manager/block_based/block_based_table_reader.h"
#include "core/sstable_manager/block_based/block_based_sstable_structs.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
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

    // uint32: 4 bytes (4 * 8 bits), eg. 0FA5
    void writeUint32BigEndian(std::ostream &out, std::uint32_t value)
    {
        unsigned char bytes[4];
        bytes[0] = static_cast<unsigned char>((value >> 24) & 0xff); // leftmost 8 bits
        bytes[1] = static_cast<unsigned char>((value >> 16) & 0xff);
        bytes[2] = static_cast<unsigned char>((value >> 8) & 0xff);
        bytes[3] = static_cast<unsigned char>(value & 0xff); // rightmost 8 bits

        out.write(reinterpret_cast<const char *>(bytes), sizeof(bytes));
        if (!out)
            throw std::runtime_error("failed to write uint32");
    }

    // writes a length-prefixed string
    // | string length N (4 bytes) | string bytes: N bytes |
    // string length is 4-byte unsigned int (unsigned int cannot be negative)
    // hence the largest encodable string is 2^32 - 1 bytes
    // hence we need to do a size check
    void writeString(std::ostream &out, const std::string &s)
    {
        if (s.size() > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::runtime_error("string too large for metadata block");
        }

        writeUint32BigEndian(out, static_cast<std::uint32_t>(s.size()));
        out.write(s.data(), static_cast<std::streamsize>(s.size()));

        if (!out)
            throw std::runtime_error("failed to write string");
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

    BlockHandle writeMetadataBlock(std::ostream &output, const SSTableMetadata &meta)
    {
        const auto start = output.tellp();
        if (start == std::streampos{-1})
            throw std::runtime_error("failed to get metadata block offset");

        writeUint64BigEndian(output, meta.m_file_number);
        writeUint64BigEndian(output, static_cast<std::uint64_t>(meta.m_timestamp));
        writeString(output, meta.m_min_key);
        writeString(output, meta.m_max_key);

        const auto end = output.tellp();
        if (end == std::streampos{-1})
            throw std::runtime_error("failed to get metadata block end");

        return BlockHandle{
            .offset = static_cast<std::uint64_t>(start),
            .size = static_cast<std::uint64_t>(end - start),
        };
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

    // 1. write metadata
    entries.SeekToFirst();
    if (!entries.Valid())
    {
        throw std::runtime_error("BlockBasedTableFormat::writeTable(): cannot write empty table");
    }

    std::string minKey = entries.Key(); // the very first entry
    std::string maxKey;
    for (; entries.Valid(); entries.Next())
    {
        maxKey = entries.Key();
    }

    SSTableMetadata meta{
        fileNum,
        timestamp,
        minKey,
        maxKey,
    };

    BlockHandle metaHandle = writeMetadataBlock(output, meta);

    // 2. write footer
    Footer footer;
    footer.metadata_block = metaHandle;
    writeFooter(output, footer);
    output.close();
    if (!output)
    {
        throw std::runtime_error("BlockBasedTableFormat: failed to close table: " + fullPath);
    }

    // create table reader and return it
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
