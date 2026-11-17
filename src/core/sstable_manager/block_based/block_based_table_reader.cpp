#include "core/sstable_manager/block_based/block_based_table_reader.h"

#include <algorithm>
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

    // fullPath = full path to the file that stores this data block
    std::optional<Entry> readEntryFromDataBlock(
        std::istream &input,
        const BlockHandle &handle,
        const std::string &key,
        const std::string &fullPath)
    {
        input.seekg(static_cast<std::streamoff>(handle.offset), std::ios::beg);
        if (!input)
        {
            throw std::runtime_error("BlockBasedTableReader: failed to seek data block: " + fullPath);
        }

        const std::streampos start = input.tellg();
        if (start == std::streampos{-1})
        {
            throw std::runtime_error("BlockBasedTableReader: failed to determine data block start: " + fullPath);
        }

        const std::uint32_t numEntries = readUint32BigEndian(input);

        // 
        for (std::uint32_t i = 0; i < numEntries; ++i)
        {
            std::string entryKey = readString(input);
            std::string entryVal = readString(input);

            char tombstoneByte = 0;
            input.read(&tombstoneByte, 1);
            if (!input)
            {
                throw std::runtime_error("BlockBasedTableReader: failed to read tombstone byte: " + fullPath);
            }

            if (entryKey == key)
            {
                return Entry{std::move(entryKey), std::move(entryVal), tombstoneByte != 0};
            }

            // Entries in a data block are sorted, so we can stop early.
            if (entryKey > key)
            {
                break;
            }
        }

        const std::streampos end = input.tellg();
        if (end == std::streampos{-1})
        {
            throw std::runtime_error("BlockBasedTableReader: failed to determine data block end: " + fullPath);
        }

        if (static_cast<std::uint64_t>(end - start) > handle.size)
        {
            throw std::runtime_error("BlockBasedTableReader: read past data block: " + fullPath);
        }

        return std::nullopt;
    }

    class BlockBasedTableIterator final : public tinykv::Iterator
    {
    public:
        BlockBasedTableIterator(const std::string &fullPath, std::vector<IndexEntry> index)
            : m_index{std::move(index)}, m_input{fullPath, std::ios::binary}
        {
            if (!m_input)
                throw std::runtime_error("BlockBasedTableReader: failed to open table: " + fullPath);
            SeekToFirst();
        }

        bool Valid() const override
        {
            return m_entryIndex < m_currentBlock.size();
        }

        void SeekToFirst() override
        {
            for (std::size_t blockIndex = 0; blockIndex < m_index.size(); ++blockIndex)
            {
                _loadBlock(blockIndex);
                if (!m_currentBlock.empty())
                {
                    m_entryIndex = 0;
                    return;
                }
            }
            _invalidate();
        }

        void SeekToLast() override
        {
            for (std::size_t blockIndex = m_index.size(); blockIndex > 0; --blockIndex)
            {
                _loadBlock(blockIndex - 1);
                if (!m_currentBlock.empty())
                {
                    m_entryIndex = m_currentBlock.size() - 1;
                    return;
                }
            }
            _invalidate();
        }

        void Seek(const std::string &target) override
        {
            const auto indexIt = std::lower_bound(
                m_index.begin(), m_index.end(), target,
                [](const IndexEntry &entry, const std::string &key)
                {
                    return entry.last_key < key;
                });
            if (indexIt == m_index.end())
            {
                _invalidate();
                return;
            }

            std::size_t blockIndex = static_cast<std::size_t>(indexIt - m_index.begin());
            for (; blockIndex < m_index.size(); ++blockIndex)
            {
                _loadBlock(blockIndex);
                const auto entryIt = std::lower_bound(
                    m_currentBlock.begin(), m_currentBlock.end(), target,
                    [](const Entry &entry, const std::string &key)
                    {
                        return entry.key < key;
                    });
                if (entryIt != m_currentBlock.end())
                {
                    m_entryIndex = static_cast<std::size_t>(entryIt - m_currentBlock.begin());
                    return;
                }
            }
            _invalidate();
        }

        void Next() override
        {
            if (!Valid())
                return;

            ++m_entryIndex;
            if (Valid())
                return;

            for (std::size_t blockIndex = m_blockIndex + 1; blockIndex < m_index.size(); ++blockIndex)
            {
                _loadBlock(blockIndex);
                if (!m_currentBlock.empty())
                {
                    m_entryIndex = 0;
                    return;
                }
            }
            _invalidate();
        }

        const std::string &Key() const override
        {
            return m_currentBlock[m_entryIndex].key;
        }

        const std::string &Value() const override
        {
            return m_currentBlock[m_entryIndex].val;
        }

        bool isTombstone() const override
        {
            return m_currentBlock[m_entryIndex].tombstone;
        }

    private:
        void _loadBlock(std::size_t blockIndex)
        {
            m_currentBlock.clear();
            m_blockIndex = blockIndex;

            const BlockHandle &handle = m_index[blockIndex].block;
            m_input.clear();
            m_input.seekg(static_cast<std::streamoff>(handle.offset), std::ios::beg);
            if (!m_input)
                throw std::runtime_error("BlockBasedTableReader: failed to seek data block");

            const std::streampos start = m_input.tellg();
            if (start == std::streampos{-1})
                throw std::runtime_error("BlockBasedTableReader: failed to determine data block start");

            const std::uint32_t numEntries = readUint32BigEndian(m_input);
            m_currentBlock.reserve(numEntries);
            for (std::uint32_t i = 0; i < numEntries; ++i)
            {
                std::string key = readString(m_input);
                std::string value = readString(m_input);
                char tombstoneByte = 0;
                m_input.read(&tombstoneByte, 1);
                if (!m_input)
                    throw std::runtime_error("BlockBasedTableReader: failed to read tombstone byte");

                m_currentBlock.push_back(Entry{std::move(key), std::move(value), tombstoneByte != 0});
            }

            const std::streampos end = m_input.tellg();
            if (end == std::streampos{-1} || static_cast<std::uint64_t>(end - start) != handle.size)
                throw std::runtime_error("BlockBasedTableReader: data block size mismatch");
            m_entryIndex = m_currentBlock.size();
        }

        void _invalidate()
        {
            m_currentBlock.clear();
            m_entryIndex = 0;
            m_blockIndex = m_index.size();
        }

        std::vector<IndexEntry> m_index;
        std::ifstream m_input;
        std::vector<Entry> m_currentBlock;
        std::size_t m_blockIndex = 0;
        std::size_t m_entryIndex = 0;
    };
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

std::string BlockBasedTableReader::getFilePath() const
{
    return m_fullPath;
}

bool BlockBasedTableReader::withinRange(const std::string &key) const
{
    return key >= m_meta.m_min_key && key <= m_meta.m_max_key;
}

// TODO: IMPLf
std::optional<Entry> BlockBasedTableReader::get(const std::string &key) const
{
    // find the corresponding index block
    // 1. find the first index entry whose `lastKey >= key`
    // this is the data block the contains `key`
    // 2. read that data block from disk
    // 3. decode entries in that block and search for `key`
    if (!withinRange(key))
    {
        return std::nullopt;
    }

    // find the first index entry wose `lastKey >= key`
    const auto it = std::lower_bound(
        m_index.begin(),
        m_index.end(),
        key,
        [](const IndexEntry &entry, const std::string &target)
        {
            return entry.last_key < target;
        });

    if (it == m_index.end())
    {
        return std::nullopt;
    }

    std::ifstream input{m_fullPath, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error("BlockBasedTableReader: failed to open table: " + m_fullPath);
    }

    return readEntryFromDataBlock(input, it->block, key, m_fullPath);
}

std::unique_ptr<tinykv::Iterator> BlockBasedTableReader::NewSortedIterator() const
{
    return std::make_unique<BlockBasedTableIterator>(m_fullPath, m_index);
}

// TODO
std::size_t BlockBasedTableReader::getSize() const
{
    return 0;
}
