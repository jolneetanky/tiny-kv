#include "core/sstable_manager/block_based/block_based_table_reader.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

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

        const auto fileSize = static_cast<std::uint64_t>(endPos);
        if (fileSize < FOOTER_ENCODED_SIZE)
        {
            throw std::runtime_error("BlockBasedTableReader: file too small for footer: " + fullPath);
        }

        input.seekg(static_cast<std::streamoff>(fileSize - FOOTER_ENCODED_SIZE), std::ios::beg); // move to start of footer block
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

    // 2. read index block

    // 3. read metadata block
}

SSTableMetadata BlockBasedTableReader::meta() const
{
}

bool BlockBasedTableReader::withinRange(const std::string &key) const
{
}

std::optional<Entry> BlockBasedTableReader::get(const std::string &key) const
{
}

std::unique_ptr<tinykv::Iterator> BlockBasedTableReader::NewIterator() const
{
}

std::size_t BlockBasedTableReader::getSize() const
{
}
