#ifndef BLOCK_BASED_TABLE_READER_H
#define BLOCK_BASED_TABLE_READER_H

#include "core/sstable_manager/sstable.h"
#include "core/sstable_manager/table_reader.h"
#include "core/sstable_manager/block_based/block_based_sstable_structs.h"

#include <memory>

class BlockBasedTableReader final : public TableReader
{
public:
    explicit BlockBasedTableReader(const std::string &fullPath);

    SSTableMetadata meta() const override;
    bool withinRange(const std::string &key) const override;
    std::optional<Entry> get(const std::string &key) const override;
    std::unique_ptr<tinykv::Iterator> NewIterator() const override;
    std::size_t getSize() const override;

private:
    std::string m_fullPath;
    Footer m_footer;
};

#endif
