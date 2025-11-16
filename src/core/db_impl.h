#ifndef DB_IMPL_H
#define DB_IMPL_H

#include "db.h"
#include "./mem_table/mem_table.h"
#include "./sstable_manager/sstable_manager.h"
#include "contexts/system_context.h"
#include "core/skip_list/skip_list_impl.h"
#include "core/wal/wal.h"
#include "core/sstable_manager/sstable_manager_impl.h"
#include "core/mem_table/mem_table_impl.h"

// interface class
class DbImpl : public DB
{
private:
    // Ownership — these are *owned* by DbImpl
    std::unique_ptr<SystemContext> m_systemCtx;
    std::unique_ptr<SkipListImpl> m_skipList;
    std::unique_ptr<WAL> m_wal;
    std::unique_ptr<SSTableManagerImpl> m_ssTableManager;
    std::unique_ptr<MemTableImpl> m_memTable;

public:
    DbImpl(std::unique_ptr<SystemContext> systemCtx,
           std::unique_ptr<SkipListImpl> skipList,
           std::unique_ptr<WAL> wal,
           std::unique_ptr<SSTableManagerImpl> sstableManager,
           std::unique_ptr<MemTableImpl> memTable);

    Response<void> put(std::string key, std::string val) override;
    Response<std::string> get(std::string key) const override;
    Response<void> del(std::string key) override;

    Response<void> forceCompactForTests(); // for testing purposes
    Response<void> forceFlushForTests();
};

#endif