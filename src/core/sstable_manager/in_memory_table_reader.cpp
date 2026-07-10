#include "core/sstable_manager/in_memory_table_reader.h"

#include <utility>

InMemoryTableReader::InMemoryTableReader(std::unique_ptr<SSTable> table) : m_table(std::move(table))
{
}

SSTableMetadata InMemoryTableReader::meta() const
{
    return m_table->meta();
}

bool InMemoryTableReader::withinRange(const std::string &key) const
{
    return m_table->withinRange(key);
}

std::optional<Entry> InMemoryTableReader::get(const std::string &key) const
{
    return m_table->get(key);
}

std::unique_ptr<tinykv::Iterator> InMemoryTableReader::NewIterator() const
{
    return m_table->NewIterator();
}

std::size_t InMemoryTableReader::getSize() const
{
    return m_table->getSize();
}
