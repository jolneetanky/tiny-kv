#ifndef MEM_TABLE_IMPL_H
#define MEM_TABLE_IMPL_H

#include <iostream>
#include "mem_table.h"
#include "../skip_list/skip_list.h"
#include "../types/entry.h"
#include "../sstable_manager/sstable_manager.h"
#include "../wal/wal.h"
#include "../contexts/system_context.h"

// from now on all entries will be represented as Entry because that makes it easier
// and i will use this as the main class ig
class MemTableImpl : public MemTable
{
    private:
        int m_size;
        bool m_readOnly = false;
        SkipList &m_skiplist;
        SSTableManager &m_ssTableManager;
        WAL &m_wal;
        SystemContext &m_systemContext;

        bool isReadOnly() const;
        std::optional<Error> flushToDisk(); // we can just do SkipList.getAll() then flush to disk

    public:
        MemTableImpl(int size, SkipList &skipList, SSTableManager &ssTableManager, WAL &wal, SystemContext &m_systemContext ); // constructor
        std::optional<Error> put(const std::string &key, const std::string &val) override;
        std::optional<Entry> get(const std::string &key) const override;
        std::optional<Error> del(const std::string &key) override;
        std::optional<Error> replayWal() override;

};

#endif