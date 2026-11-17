#ifndef DB_IMPL_H
#define DB_IMPL_H

#include "db.h"
#include "./mem_table/mem_table.h"
#include "contexts/system_context.h"
#include "core/skip_list/skip_list_impl.h"
#include "core/wal/wal.h"
#include "core/storage_manager/storage_manager.h"
#include "core/mem_table/mem_table_impl.h"
#include <condition_variable>
#include <mutex>
#include <thread>

class DbImpl : public DB
{
private:
    // Ownership — these are *owned* by DbImpl
    std::unique_ptr<SystemContext> m_systemCtx;
    std::unique_ptr<SkipListImpl> m_skipList;
    std::unique_ptr<WAL> m_wal;
    std::unique_ptr<StorageManager> m_storageManager;
    std::unique_ptr<MemTableImpl> m_memTable;

    mutable std::mutex m_dbMutex;
    std::condition_variable m_compactionCv;
    bool m_compactionRequested = false;
    bool m_stopCompactionWorker = false;
    std::thread m_compactionWorker;

    void compactionWorkerLoop();
    void requestCompactionIfNeeded();

public:
    DbImpl(std::unique_ptr<SystemContext> systemCtx,
           std::unique_ptr<SkipListImpl> skipList,
           std::unique_ptr<WAL> wal,
           std::unique_ptr<StorageManager> storageManager,
           std::unique_ptr<MemTableImpl> memTable);
    ~DbImpl() override;

    protocol::Response put(std::string key, std::string val) override;
    protocol::Response get(std::string key) const override;
    protocol::Response del(std::string key) override;

    protocol::Response forceCompactForTests() override; // for testing purposes; force memtable compaction
    protocol::Response forceFlushForTests() override;
};

#endif
