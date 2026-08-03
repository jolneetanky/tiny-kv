#ifndef IN_MEMORY_TABLE_READER_H
#define IN_MEMORY_TABLE_READER_H

#include "core/sstable_manager/sstable.h"
#include "core/sstable_manager/table_reader.h"

#include <memory>

class InMemoryTableReader final : public TableReader
{
public:
    explicit InMemoryTableReader(std::unique_ptr<SSTable> table);

    SSTableMetadata meta() const override;
    bool withinRange(const std::string &key) const override;
    std::optional<Entry> get(const std::string &key) const override;
    std::unique_ptr<tinykv::Iterator> NewIterator() const override;
    std::size_t getSize() const override;

private:
    std::unique_ptr<SSTable> m_table;
};

#endif
