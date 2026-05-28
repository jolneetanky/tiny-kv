#include "db_impl.h"
#include <iostream>
#include <optional>
#include "../types/entry.h"

DbImpl::DbImpl(std::unique_ptr<SystemContext> ctx,
               std::unique_ptr<SkipListImpl> skip,
               std::unique_ptr<WAL> wal,
               std::unique_ptr<StorageManagerImpl> sm,
               std::unique_ptr<MemTableImpl> mem)
    : m_systemCtx(std::move(ctx)), m_skipList(std::move(skip)), m_wal(std::move(wal)), m_storageManager(std::move(sm)), m_memTable(std::move(mem))
{
}

protocol::Response DbImpl::put(std::string key, std::string val)
{
    std::optional<Error> errOpt{(*m_memTable).put(key, val)};

    if (errOpt)
    {
        return protocol::Response{false, errOpt->error, std::nullopt};
    }

    return protocol::Response{true, "OK", std::nullopt};
}

protocol::Response DbImpl::get(std::string key) const
{
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
    std::optional<Error> errOpt{m_memTable->del(key)};

    if (errOpt)
    {
        return protocol::Response{false, "Failed to DELETE key: " + errOpt->error, std::nullopt};
    }

    return protocol::Response{true, "OK", std::nullopt};
}

protocol::Response DbImpl::forceCompactForTests()
{
    m_storageManager->compact();
    return protocol::Response{true, "OK", std::nullopt};
}

protocol::Response DbImpl::forceFlushForTests()
{
    m_memTable->flushToDisk();
    return protocol::Response{true, "OK", std::nullopt};
}
