#ifndef BLOCK_BASED_SSTABLE_STRUCTS_H
#define BLOCK_BASED_SSTABLE_STRUCTS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

inline constexpr std::size_t BLOCK_HANDLE_ENCODED_SIZE = 16;
inline constexpr std::size_t FOOTER_ENCODED_SIZE = 3 * BLOCK_HANDLE_ENCODED_SIZE;

// size = 2 * 8 bytes
// = 16 bytes
struct BlockHandle
{
    std::uint64_t offset = 0; // 64 bits (8 bytes)
    std::uint64_t size = 0;   // 64 bits (8 bytes)
};

// One index entry points to one data block.
// `last_key` is the largest key in that data block; lookup can find the first
// index entry whose last_key is >= the searched key.
// BlockBasedSSTableReader stores a `std::vector<IndexEntry>`.
struct IndexEntry
{
    std::string last_key; // largest key in this data block
    BlockHandle block;    // BlockHandle of the data block
};

// Decoded Bloom filter block loaded by BlockBasedTableReader.
struct FilterBlock
{
    std::uint32_t num_bits = 0;
    std::uint32_t num_hashes = 0;
    std::vector<unsigned char> bitset;
};

// fixed size footer at the end of each file
// encoded size = 3 * 16 bytes
// = 48 bytes
struct Footer
{
    BlockHandle metadata_block; // 16 bytes
    BlockHandle index_block;    // 16 bytes
    BlockHandle filter_block;   // 16 bytes
};

#endif
