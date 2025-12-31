#ifndef SSTABLE_META_H
#define SSTABLE_META_H

#include <stdint.h>
#include "types/timestamp.h"
#include "types/types.h"
#include <string>

// This metadata should stay the same regardless of the SSTable implementation.
struct SSTableMetadata
{
    FileNumber m_file_number;
    TimestampType m_timestamp;
    std::string m_min_key;
    std::string m_max_key;

    // ordering rules for compaction
    bool operator<(const SSTableMetadata &other) const
    {
        return this->m_file_number < other.m_file_number;
    }

    SSTableMetadata() = default;
    SSTableMetadata(FileNumber file_number, TimestampType timestamp, std::string min_key, std::string max_key) : m_file_number{file_number}, m_timestamp{timestamp}, m_min_key{min_key}, m_max_key{max_key} {};
};

#endif