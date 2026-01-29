#include "core/sstable_manager/sstable.h"
#include <iostream>
#include <utility>

SSTable::SSTable(SSTableMetadata meta, std::vector<Entry> &&entries) : m_meta{meta}, m_entries{std::move(entries)}
{
    std::sort(m_entries.begin(), m_entries.end());
};

SSTableMetadata SSTable::meta() const
{
    return m_meta;
}; // returns a copy of SSTableMetadata so this class itself remains unchanged.

std::optional<Entry> SSTable::get(std::string key) const
{
    if (m_entries.size() == 0)
    {
        return std::nullopt;
    }

    // binary search :)
    int l = 0;
    int r = m_entries.size() - 1;

    // assume within an SSTable there are no duplicate keys.
    while (l < r)
    {
        int mid = l + ((r - l) / 2);
        const Entry &midEntry = m_entries[mid];

        if (midEntry.key == key)
        {
            return midEntry;
        }

        if (key < midEntry.key)
        {
            r = mid - 1;
        }
        else
        {
            l = mid + 1;
        }
    }

    if (m_entries[l].key == key)
    {
        return m_entries[l];
    }

    return std::nullopt;
};

// TODO: implement this
bool SSTable::contains(std::string key) const
{
    return true;
};

std::string SSTable::getStartKey() const
{
    return m_entries[0].key;
};

std::string SSTable::getEndKey() const
{
    return m_entries[m_entries.size() - 1].key;
};

std::size_t SSTable::getSize() const
{
    return m_entries.size();
}

std::span<const Entry> SSTable::getEntries() const
{
    return m_entries;
}

// Why not return the iterator by value?
//
std::unique_ptr<tinykv::Iterator> SSTable::NewIterator() const
{
    auto it = std::make_unique<SSTable::Iter>(this);
    return std::move(it);
};