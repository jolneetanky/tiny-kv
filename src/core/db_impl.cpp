#include "db_impl.h"
#include <iostream>
#include <optional>
#include "../types/entry.h"
#include "../common/log.h"

DbImpl::DbImpl(std::unique_ptr<SystemContext> ctx,
               std::unique_ptr<SkipListImpl> skip,
               std::unique_ptr<WAL> wal,
               std::unique_ptr<StorageManagerImpl> sm,
               std::unique_ptr<MemTableImpl> mem)
    : m_systemCtx(std::move(ctx)), m_skipList(std::move(skip)), m_wal(std::move(wal)), m_storageManager(std::move(sm)), m_memTable(std::move(mem))
{
    m_compactionWorker = std::thread(&DbImpl::compactionWorkerLoop, this);
}

DbImpl::~DbImpl()
{
    {
        std::lock_guard<std::mutex> lock(m_dbMutex);
        m_stopCompactionWorker = true;
    }
    m_compactionCv.notify_one();
    if (m_compactionWorker.joinable())
        m_compactionWorker.join();
}

void DbImpl::requestCompactionIfNeeded()
{
    // if
    // 1. no compaction needed, OR
    // 2. compaction needed, but this is a duplicate compaction request to the worker
    if (!m_storageManager->isCompactionNeeded() || m_compactionRequested)
        return;

    m_compactionRequested = true;
    m_compactionCv.notify_one();
}

void DbImpl::compactionWorkerLoop()
{
    std::unique_lock<std::mutex> lock(m_dbMutex);
    while (true)
    {
        // `condition_variable::wait()` handles spurious wakeups alr
        // releases the mutex while waiting
        m_compactionCv.wait(lock, [this]
                            { return m_stopCompactionWorker || m_compactionRequested; });
        if (m_stopCompactionWorker)
            return;

        m_storageManager->compact();
        m_compactionRequested = false; // only mark this completion request as done AFTER compact() is complete
    }
}

protocol::Response DbImpl::put(std::string key, std::string val)
{
    std::lock_guard<std::mutex> lock(m_dbMutex);
    TINYKV_LOG("[DbImpl.put()]");
    std::optional<Error> errOpt{(*m_memTable).put(key, val)};
    requestCompactionIfNeeded();

    if (errOpt)
    {
        return protocol::Response{false, errOpt->error, std::nullopt};
    }

    return protocol::Response{true, "OK", std::nullopt};
}

protocol::Response DbImpl::get(std::string key) const
{
    std::lock_guard<std::mutex> lock(m_dbMutex);
    TINYKV_LOG("[DbImpl.get()]");
    std::optional<Entry> optEntry{(*m_memTable).get(key)};

    if (optEntry && optEntry->tombstone)
    {
        return protocol::Response{false, "Key does not exist", std::nullopt};
    }

    if (!optEntry)
    {
        optEntry = (*m_storageManager).get(key);

        if (!optEntry)
        {
            return protocol::Response{false, "Key does not exist", std::nullopt};
        }
    }

    return protocol::Response{true, "OK", optEntry.value().val};
}

protocol::Response DbImpl::del(std::string key)
{
    std::lock_guard<std::mutex> lock(m_dbMutex);
    TINYKV_LOG("[DbImpl.del()]");
    std::optional<Error> errOpt{m_memTable->del(key)};
    requestCompactionIfNeeded();

    if (errOpt)
    {
        return protocol::Response{false, "Failed to DELETE key: " + errOpt->error, std::nullopt};
    }

    return protocol::Response{true, "OK", std::nullopt};
}

protocol::Response DbImpl::forceCompactForTests()
{
    std::lock_guard<std::mutex> lock(m_dbMutex);
    TINYKV_LOG("[DbImpl.forceCompactForTests()]");
    m_storageManager->compact();
    return protocol::Response{true, "OK", std::nullopt};
}

protocol::Response DbImpl::forceFlushForTests()
{
    std::lock_guard<std::mutex> lock(m_dbMutex);
    TINYKV_LOG("[DbImpl.forceFlushForTests()]");
    m_memTable->flushToDisk();
    requestCompactionIfNeeded();
    return protocol::Response{true, "OK", std::nullopt};
}
