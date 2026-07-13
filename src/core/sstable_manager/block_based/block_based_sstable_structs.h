#ifndef BLOCK_BASED_SSTABLE_STRUCTS_H
#define BLOCK_BASED_SSTABLE_STRUCTS_H

#include <cstddef>
#include <cstdint>

inline constexpr std::size_t BLOCK_HANDLE_ENCODED_SIZE = 16;
inline constexpr std::size_t FOOTER_ENCODED_SIZE = 3 * BLOCK_HANDLE_ENCODED_SIZE;

// size = 2 * 8 bytes
// = 16 bytes
struct BlockHandle
{
    std::uint64_t offset = 0; // 64 bits (8 bytes)
    std::uint64_t size = 0;   // 64 bits (8 bytes)
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
