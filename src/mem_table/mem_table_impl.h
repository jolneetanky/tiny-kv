#ifndef MEM_TABLE_IMPL_H
#define MEM_TABLE_IMPL_H

#include <iostream>
#include "mem_table.h"
#include "../skip_list/skip_list.h"
#include "../types/entry.h"
#include "../sstable_manager/sstable_manager.h"

// from now on all entries will be represented as Entry because that makes it easier
// and i will use this as the main class ig
class MemTableImpl : public MemTable
{
    private:
    SkipList &m_skiplist;
    int m_size;
    bool m_readOnly = false;
    SSTableManager &m_ssTableManager;

public:
    MemTableImpl(int size, SkipList &skipList, SSTableManager &ssTableManager); // constructor
    Error* put(const std::string &key, const std::string &val) override;
    const Entry* get(const std::string &key) const override;
    void del(const std::string &key) override;
    bool isReadOnly() override;

    void flushToDisk() override; // we can just do SkipList.getAll() then flush to disk
};

#endif