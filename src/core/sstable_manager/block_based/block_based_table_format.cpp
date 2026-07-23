#include "core/sstable_manager/block_based/block_based_table_format.h"

#include "core/sstable_manager/block_based/block_based_table_reader.h"
#include "core/sstable_manager/block_based/block_based_sstable_structs.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

// #include "core/sstable_manager/sstable.h"
// #include "core/sstable_manager/sstable_reader.h"
// #include "core/sstable_manager/sstable_writer.h"
constexpr std::size_t DATA_BLOCK_MAX_SIZE = 4096; // max number of bytes in data block
constexpr std::uint32_t FILTER_NUM_BITS = 8192;
constexpr std::uint32_t FILTER_NUM_HASHES = 7;

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
    // write uint32 directly to a stream (eg. a file)
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

    // 1. data block helpers
    // append uint32 to a std::string
    // convert uint32 into a char array
    // eg. 7 = 111 -> becomes [0, 0, 0, 7]
    // basically split up uint32 from full 4 bytes into an array of [1 byte, 1 byte, 1 byte, 1 byte]
    void appendUint32BigEndian(std::string &out, std::uint32_t value)
    {
        char bytes[4];
        bytes[0] = static_cast<char>((value >> 24) & 0xff);
        bytes[1] = static_cast<char>((value >> 16) & 0xff);
        bytes[2] = static_cast<char>((value >> 8) & 0xff);
        bytes[3] = static_cast<char>(value & 0xff);
        out.append(bytes, sizeof(bytes));
    }

    // convert the `string`
    // appends "<length> <string>" to another `std::string`
    void appendString(std::string &out, const std::string &s)
    {
        if (s.size() > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::runtime_error("string too large for data block");
        }

        appendUint32BigEndian(out, static_cast<std::uint32_t>(s.size()));
        out.append(s);
    }

    void appendEntry(std::string &out, const std::string &key, const std::string &val, bool tombstone)
    {
        appendString(out, key);
        appendString(out, tombstone ? "" : val);

        const char tombstoneByte = tombstone ? 1 : 0;
        out.append(&tombstoneByte, 1);
    }

    BlockHandle writeRawBlock(std::ostream &output, const std::string &block)
    {
        const auto start = output.tellp();
        if (start == std::streampos{-1})
        {
            throw std::runtime_error("failed to get block offset");
        }

        output.write(block.data(), static_cast<std::streamsize>(block.size()));
        if (!output)
        {
            throw std::runtime_error("failed to write block");
        }

        return BlockHandle{
            .offset = static_cast<std::uint64_t>(start),
            .size = static_cast<std::uint64_t>(block.size()),
        };
    }

    void writeBlockHandle(std::ostream &output, const BlockHandle &handle)
    {
        writeUint64BigEndian(output, handle.offset);
        writeUint64BigEndian(output, handle.size);
    }

    // 2. metadata helpers
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

    // 3. index helpers
    // write all IndexEntries into one index block
    // the index block looks like:
    // <numEntries><lastKey1><blockHandle1>...
    BlockHandle writeIndexBlock(std::ostream &output, const std::vector<IndexEntry> &index)
    {
        const auto start = output.tellp();
        if (start == std::streampos{-1})
        {
            throw std::runtime_error("failed to get index block offset");
        }

        if (index.size() > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::runtime_error("too many index entries");
        }

        writeUint32BigEndian(output, static_cast<std::uint32_t>(index.size()));

        for (const IndexEntry &entry : index)
        {
            writeString(output, entry.last_key);
            writeBlockHandle(output, entry.block);
        }

        const auto end = output.tellp();
        if (end == std::streampos{-1})
        {
            throw std::runtime_error("failed to get index block end");
        }

        return BlockHandle{
            .offset = static_cast<std::uint64_t>(start),
            .size = static_cast<std::uint64_t>(end - start),
        };
    }

    // 4. filter helpers
    // The filter block is a serialized Bloom filter for all keys in this table.
    //
    // On disk, the filter block is:
    // | num_bits:        uint32 big-endian |
    // | num_hashes:      uint32 big-endian |
    // | bitset_nbytes:   uint32 big-endian |
    // | bitset bytes:    bitset_nbytes raw bytes |
    //
    // The bitset is packed little-bit-order inside each byte:
    // bit index i is stored at byte i / 8, mask 1 << (i % 8).
    //
    // The footer stores this block's location as:
    // footer.filter_block.offset = where this serialized filter starts
    // footer.filter_block.size   = total bytes written for the filter block
    std::uint64_t hashKeyWithSeed(const std::string &key, std::uint64_t seed)
    {
        std::uint64_t hash = 1469598103934665603ull ^ seed;
        for (unsigned char byte : key)
        {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    // add a key to the bloom filter
    // bloom filter is represented by the bitset
    // directly modify `bitset`
    // for each key,
    // 1. compute several hash positions. This number is determined by `FILTER_NUM_HASHES`
    // if `FILTER_NUM_HASHES = 3`, then every key maps to 3 bit positions.
    void addKeyToFilter(std::vector<unsigned char> &bitset, const std::string &key)
    {
        for (std::uint32_t seed = 0; seed < FILTER_NUM_HASHES; ++seed)
        {
            // eg.
            // hash(key, 0) = 12
            // hash(key, 1) = 9
            // hash(key, 2) = 3
            // so for this `key`, we set bit positions 3, 9, 12 to 1.
            const std::uint64_t hash = hashKeyWithSeed(key, seed);
            const std::uint32_t bitIndex = static_cast<std::uint32_t>(hash % FILTER_NUM_BITS);
            bitset[bitIndex / 8] |= static_cast<unsigned char>(1u << (bitIndex % 8));
        }
    }

    // Writes one serialized Bloom filter block to `output`.
    // The bytes written are, in order:
    // 1. FILTER_NUM_BITS as uint32 big-endian: total number of logical Bloom bits.
    // 2. FILTER_NUM_HASHES as uint32 big-endian: number of hash probes per key.
    // 3. bitset.size() as uint32 big-endian: number of raw bytes that follow.
    // 4. bitset bytes: packed Bloom bits, where bit i lives at byte i / 8
    //    and mask 1 << (i % 8).
    //
    // So the file block looks like:
    // | num_bits: 4 bytes | num_hashes: 4 bytes | bitset_nbytes: 4 bytes | bitset bytes |
    // The returned BlockHandle points to the start of `num_bits` and spans all
    // bytes through the end of the bitset.
    BlockHandle writeFilterBlock(std::ostream &output, const std::vector<unsigned char> &bitset)
    {
        const auto start = output.tellp();
        if (start == std::streampos{-1})
        {
            throw std::runtime_error("failed to get filter block offset");
        }

        if (bitset.size() > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::runtime_error("filter block bitset too large");
        }

        writeUint32BigEndian(output, FILTER_NUM_BITS);
        writeUint32BigEndian(output, FILTER_NUM_HASHES);
        writeUint32BigEndian(output, static_cast<std::uint32_t>(bitset.size()));
        output.write(reinterpret_cast<const char *>(bitset.data()), static_cast<std::streamsize>(bitset.size()));
        if (!output)
        {
            throw std::runtime_error("failed to write filter block");
        }

        const auto end = output.tellp();
        if (end == std::streampos{-1})
        {
            throw std::runtime_error("failed to get filter block end");
        }

        return BlockHandle{
            .offset = static_cast<std::uint64_t>(start),
            .size = static_cast<std::uint64_t>(end - start),
        };
    }

    // 5. footer helpers
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

    // 1. write data blocks
    entries.SeekToFirst();
    if (!entries.Valid())
    {
        throw std::runtime_error("BlockBasedTableFormat::writeTable(): cannot write empty table");
    }

    std::string minKey = entries.Key(); // the very first entry
    std::string maxKey;
    std::vector<IndexEntry> index; // stores all IndexEntry of the current SSTable, one per data block
    std::string dataBlock;         // stores the current encoded entries for the current data block
    std::uint32_t entriesInBlock = 0;
    std::string lastKeyInBlock;
    std::vector<unsigned char> filterBitset((FILTER_NUM_BITS + 7) / 8, 0); // stores bloom filter bits

    auto flushDataBlock = [&]()
    {
        if (entriesInBlock == 0)
        {
            return;
        }

        std::string encodedBlock;
        appendUint32BigEndian(encodedBlock, entriesInBlock);
        encodedBlock.append(dataBlock);

        BlockHandle dataBlockHandle = writeRawBlock(output, encodedBlock);
        index.push_back(IndexEntry{
            .last_key = lastKeyInBlock,
            .block = dataBlockHandle,
        });

        dataBlock.clear();
        entriesInBlock = 0;
        lastKeyInBlock.clear();
    };

    for (; entries.Valid(); entries.Next())
    {
        std::string encodedEntry; // encode into bytes
        appendEntry(encodedEntry, entries.Key(), entries.Value(), entries.isTombstone());

        // if the current entry can't fit in the current data block
        if (entriesInBlock > 9 && dataBlock.size() + encodedEntry.size() > DATA_BLOCK_MAX_SIZE)
        {
            flushDataBlock();
        }

        // now can write to dataBlock
        dataBlock.append(encodedEntry);
        ++entriesInBlock;
        addKeyToFilter(filterBitset, entries.Key());

        lastKeyInBlock = entries.Key();
        maxKey = entries.Key();
    }

    flushDataBlock();

    // 2. write metadata

    SSTableMetadata meta{
        fileNum,
        timestamp,
        minKey,
        maxKey,
    };

    BlockHandle metaHandle = writeMetadataBlock(output, meta);

    // 3. write index block
    BlockHandle indexHandle = writeIndexBlock(output, index);

    // 4. write filter block
    BlockHandle filterHandle = writeFilterBlock(output, filterBitset);

    // 5. write footer
    Footer footer;
    footer.metadata_block = metaHandle;
    footer.index_block = indexHandle;
    footer.filter_block = filterHandle;
    writeFooter(output, footer);
    output.close();
    if (!output)
    {
        throw std::runtime_error("BlockBasedTableFormat: failed to close table: " + fullPath);
    }

    // create table reader and return it (ie. store the structures in memory)
    return std::make_shared<BlockBasedTableReader>(fullPath);
}
