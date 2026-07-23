#include "core/sstable_manager/block_based/block_based_table_reader.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    // reads a 64-bit (8-byte) guy from disk into memory as std::uint64_t
    std::uint64_t readUint64BigEndian(std::istream &input)
    {
        std::array<unsigned char, sizeof(std::uint64_t)> bytes{};         // create an array of size 8 bytes
        input.read(reinterpret_cast<char *>(bytes.data()), bytes.size()); // read exactly 8 bytes from `input` into the `bytes` array
        if (!input)
        {
            throw std::runtime_error("failed to read uint64");
        }

        std::uint64_t value = 0;
        // for each byte (8 bits),
        // shift the current value left by 8 bits,
        // then add the new byte into the low 8 bits
        for (unsigned char byte : bytes)
        {
            value = (value << 8) | byte;
        }

        return value;
    }

    std::uint32_t readUint32BigEndian(std::istream &input)
    {
        std::array<unsigned char, sizeof(std::uint32_t)> bytes{};
        input.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
        if (!input)
        {
            throw std::runtime_error("failed to read uint32");
        }

        std::uint32_t value = 0;
        for (unsigned char byte : bytes)
        {
            value = (value << 8) | byte;
        }

        return value;
    }

    std::string readString(std::istream &input)
    {
        const std::uint32_t size = readUint32BigEndian(input);

        std::string value;
        value.resize(size);

        input.read(value.data(), static_cast<std::streamsize>(size));
        if (!input)
        {
            throw std::runtime_error("failed to read string");
        }

        return value;
    }

    BlockHandle readBlockHandle(std::istream &input)
    {
        return BlockHandle{
            .offset = readUint64BigEndian(input),
            .size = readUint64BigEndian(input),
        };
    }

    // on disk, the footer is really just
    // metadata.offset, metadata.size, index.offset, index.size, filter.offset, filter.size
    Footer readFooter(std::istream &input, const std::string &fullPath)
    {
        input.seekg(0, std::ios::end);
        const std::streampos endPos = input.tellg(); // end position of the file
        if (endPos == std::streampos{-1})
        {
            throw std::runtime_error("BlockBasedTableReader: failed to determine table size: " + fullPath);
        }

        if (endPos < static_cast<std::streamoff>(FOOTER_ENCODED_SIZE))
        {
            throw std::runtime_error("BlockBasedTableReader: file too small for footer: " + fullPath);
        }

        input.seekg(-static_cast<std::streamoff>(FOOTER_ENCODED_SIZE), std::ios::end); // move to start of footer block
        if (!input)
        {
            throw std::runtime_error("BlockBasedTableReader: failed to seek footer: " + fullPath);
        }

        // order matters here
        // read from top to bottom of footer
        Footer footer;
        footer.metadata_block = readBlockHandle(input);
        footer.index_block = readBlockHandle(input);
        footer.filter_block = readBlockHandle(input);
        return footer;
    }

    SSTableMetadata readMetadataBlock(std::istream &input, const BlockHandle &handle, const std::string &fullPath)
    {
        if (handle.offset == 0 && handle.size == 0)
        {
            throw std::runtime_error("BlockBasedTableReader: missing metadata block: " + fullPath);
        }

        // seek to this offset
        input.seekg(static_cast<std::streamoff>(handle.offset), std::ios::beg);
        if (!input)
        {
            throw std::runtime_error("BlockBasedTableReader: failed to seek metadata block: " + fullPath);
        }

        const std::streampos start = input.tellg();
        if (start == std::streampos{-1})
        {
            throw std::runtime_error("BlockBasedTableReader: failed to determine metadata block start: " + fullPath);
        }

        const FileNumber fileNumber = readUint64BigEndian(input);
        const TimestampType timestamp = static_cast<TimestampType>(readUint64BigEndian(input));
        std::string minKey = readString(input);
        std::string maxKey = readString(input);

        const std::streampos end = input.tellg();
        if (end == std::streampos{-1})
        {
            throw std::runtime_error("BlockBasedTableReader: failed to determine metadata block end: " + fullPath);
        }

        const auto bytesRead = static_cast<std::uint64_t>(end - start);
        if (bytesRead != handle.size)
        {
            throw std::runtime_error("BlockBasedTableReader: metadata block size mismatch: " + fullPath);
        }

        return SSTableMetadata{
            fileNumber,
            timestamp,
            std::move(minKey),
            std::move(maxKey),
        };
    }

    std::vector<IndexEntry> readIndexBlock(std::istream &input, const BlockHandle &handle, const std::string &fullPath)
    {
        if (handle.offset == 0 && handle.size == 0)
        {
            throw std::runtime_error("BlockBasedTableReader: missing index block: " + fullPath);
        }

        input.seekg(static_cast<std::streamoff>(handle.offset), std::ios::beg);
        if (!input)
        {
            throw std::runtime_error("BlockBasedTableReader: failed to seek index block: " + fullPath);
        }

        const std::streampos start = input.tellg();
        if (start == std::streampos{-1})
        {
            throw std::runtime_error("BlockBasedTableReader: failed to determine index block start: " + fullPath);
        }

        const std::uint32_t numEntries = readUint32BigEndian(input);
        std::vector<IndexEntry> index;
        index.reserve(numEntries);

        for (std::uint32_t i = 0; i < numEntries; ++i)
        {
            index.push_back(IndexEntry{
                .last_key = readString(input),
                .block = readBlockHandle(input),
            });
        }

        const std::streampos end = input.tellg();
        if (end == std::streampos{-1})
        {
            throw std::runtime_error("BlockBasedTableReader: failed to determine index block end: " + fullPath);
        }

        const auto bytesRead = static_cast<std::uint64_t>(end - start);
        if (bytesRead != handle.size)
        {
            throw std::runtime_error("BlockBasedTableReader: index block size mismatch: " + fullPath);
        }

        return index;
    }

    FilterBlock readFilterBlock(std::istream &input,
                                const BlockHandle &handle,
                                const std::string &fullPath)
    {
        if (handle.offset == 0 && handle.size == 0)
        {
            throw std::runtime_error("BlockBasedTableReader: missing filter block: " + fullPath);
        }

        input.seekg(static_cast<std::streamoff>(handle.offset), std::ios::beg);
        if (!input)
        {
            throw std::runtime_error("BlockBasedTableReader: failed to seek filter block: " + fullPath);
        }

        const std::streampos start = input.tellg();
        if (start == std::streampos{-1})
        {
            throw std::runtime_error("BlockBasedTableReader: failed to determine filter block start: " + fullPath);
        }

        FilterBlock filter;
        filter.num_bits = readUint32BigEndian(input);
        filter.num_hashes = readUint32BigEndian(input);
        const std::uint32_t bitsetSize = readUint32BigEndian(input);

        filter.bitset.resize(bitsetSize);
        input.read(reinterpret_cast<char *>(filter.bitset.data()), static_cast<std::streamsize>(filter.bitset.size()));
        if (!input)
        {
            throw std::runtime_error("BlockBasedTableReader: failed to read filter bitset: " + fullPath);
        }

        const std::streampos end = input.tellg();
        if (end == std::streampos{-1})
        {
            throw std::runtime_error("BlockBasedTableReader: failed to determine filter block end: " + fullPath);
        }

        const auto bytesRead = static_cast<std::uint64_t>(end - start);
        if (bytesRead != handle.size)
        {
            throw std::runtime_error("BlockBasedTableReader: filter block size mismatch: " + fullPath);
        }

        return filter;
    }
}

BlockBasedTableReader::BlockBasedTableReader(const std::string &fullPath)
    : m_fullPath{fullPath}
{
    std::ifstream input{m_fullPath, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error("BlockBasedTableReader: failed to open table: " + m_fullPath);
    }

    // 1. read footer (offset + size of index, metadata, and filter blocks)
    m_footer = readFooter(input, m_fullPath);

    // 2. read filter block
    m_filter = readFilterBlock(input, m_footer.filter_block, m_fullPath);

    // 3. read index block
    m_index = readIndexBlock(input, m_footer.index_block, m_fullPath);

    // 4. read metadata block (SSTableMetadata)
    m_meta = readMetadataBlock(input, m_footer.metadata_block, m_fullPath);
}

SSTableMetadata BlockBasedTableReader::meta() const
{
    return m_meta;
}

bool BlockBasedTableReader::withinRange(const std::string &key) const
{
    return key >= m_meta.m_min_key && key <= m_meta.m_max_key;
}

std::optional<Entry> BlockBasedTableReader::get(const std::string &key) const
{
    (void)key;
    return std::nullopt;
}

std::unique_ptr<tinykv::Iterator> BlockBasedTableReader::NewIterator() const
{
    throw std::runtime_error("BlockBasedTableReader::NewIterator(): data blocks are not implemented yet");
}

std::size_t BlockBasedTableReader::getSize() const
{
    return 0;
}
