#include "core/sstable_manager/sstable.h"
#include "core/iterators/iterator.h"
#include <algorithm>

SSTable::Iter::Iter(const SSTable *table)
    : m_index(0), m_ssTable(table) {};

bool SSTable::Iter::Valid() const
{
    // valid iff index < table size
    // return m_index < m_ssTable ->
    return m_index < m_ssTable->m_entries.size();
};

void SSTable::Iter::SeekToFirst()
{
    m_index = 0;
};

// If empty, index is set to 0 (which is invalid because valid iff index < table size)
void SSTable::Iter::SeekToLast()
{
    if (m_ssTable->m_entries.empty())
    {
        m_index = 0;
    }
    else
    {

        m_index = m_ssTable->m_entries.size() - 1;
    }
};

// Moves to the first position with matching target key.
// If key is not found, iterator ends up in the last position and is no longer Valid().
void SSTable::Iter::Seek(const std::string &key)
{

    if (m_ssTable->m_entries.size() == 0)
    {
        m_index = m_ssTable->m_entries.size();
        return;
    }

    // binary search :)
    int l = 0;
    int r = m_ssTable->m_entries.size() - 1;

    // assume within an SSTable there are no duplicate keys.
    m_index = m_ssTable->m_entries.size(); // set invalid first

    while (l <= r)
    {
        int mid = l + ((r - l) / 2);
        const Entry &midEntry = m_ssTable->m_entries[mid];

        if (midEntry.key == key)
        {
            m_index = mid;
            break;
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
};

// Advances the iterator to the next key in the source.
// After this, the iterator is Valid() iff we haven't reached the end of the source.
void SSTable::Iter::Next()
{
    if (m_index < m_ssTable->m_entries.size())
    {
        ++m_index;
    }
};

// ACCESSORS
// REQUIRES: Valid()

// Return the key for the current entry pointed to.
const std::string &SSTable::Iter::Key() const
{
    return m_ssTable->m_entries[m_index].key;
};

// Return the value for the current entry pointed to.
const std::string &SSTable::Iter::Value() const
{
    return m_ssTable->m_entries[m_index].val;
};

bool SSTable::Iter::isTombstone() const
{
    return m_ssTable->m_entries[m_index].tombstone;
};
