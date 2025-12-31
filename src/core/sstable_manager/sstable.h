#ifndef SSTABLE_H
#define SSTABLE_H

#include "core/sstable_manager/sstable_metadata.h"
#include "types/entry.h"

/*
This class manages an SSTable on disk.
It serves as the logical representation of an SSTable on disk.

Should this class have a static method to initialize from disk?
-- No, that's not the responsibility of this class. This class simply represents an SSTable
and we can do stuff like read its metadata
and it also provides a custom comparator.
--
*/

/*
This class represents an SSTable file.

INVARIANTS:
1. Assume every SSTable file has no duplicate keys.
2. Assume this SSTable represents an actual SSTAble file that exists on disk. It is the caller's responsibility to ensure this.
*/
class SSTable
{
private:
    SSTableMetadata m_meta;
    std::vector<Entry> m_entries;

public:
    SSTable(SSTableMetadata meta, std::vector<Entry> &&entries);
    SSTableMetadata meta() const; // returns a copy of SSTableMetadata so this class itself remains unchanged.
    bool contains(std::string key) const;
    std::optional<Entry> get(std::string key) const;

    // delete copy ctor and copy assignment operator
    // we don't want an SSTable to be copied.
    SSTable(const SSTable &) = delete;
    SSTable &operator=(const SSTable &) = delete;

    // need to define move ctor and move assignment operators
    // because the compiler won't define it if we defined the copy ctor and assignment (which we did when we deleted them)
    SSTable(SSTable &&) noexcept = default;
    SSTable &operator=(SSTable &&) noexcept = default;
};

#endif