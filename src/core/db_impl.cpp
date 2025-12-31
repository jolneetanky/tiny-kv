#include "db_impl.h"
#include <iostream>
#include <optional>
#include "../types/entry.h"

DbImpl::DbImpl(std::unique_ptr<SystemContext> ctx,
               std::unique_ptr<SkipListImpl> skip,
               std::unique_ptr<WAL> wal,
               std::unique_ptr<DiskManagerImpl> dm,
               std::unique_ptr<MemTableImpl> mem)
    : m_systemCtx(std::move(ctx)), m_skipList(std::move(skip)), m_wal(std::move(wal)), m_diskManager(std::move(dm)), m_memTable(std::move(mem))
{
}

Response<void> DbImpl::put(std::string key, std::string val)
{
    std::optional<Error> errOpt{(*m_memTable).put(key, val)};

    if (errOpt)
    {
        return Response<void>(false, errOpt->error);
    }

    return Response<void>(true, "Successfully PUT key " + key + " in DB");
}

Response<std::string> DbImpl::get(std::string key) const
{
    std::optional<Entry> optEntry{(*m_memTable).get(key)};

    if (optEntry && optEntry->tombstone)
    {
        return Response<std::string>(false, "Key does not exist", std::nullopt);
    }

    if (!optEntry)
    {
        optEntry = (*m_diskManager).get(key);

        if (!optEntry)
        {
            return Response<std::string>(false, "Key does not exist", std::nullopt);
        }
    }

    return Response<std::string>(true, "", optEntry.value().val);
}

Response<void> DbImpl::del(std::string key)
{
    std::optional<Error> errOpt{m_memTable->del(key)};

    if (errOpt)
    {
        // std::cout << "[DbImpl.del] Failed to DELETE key: " << errOpt->error << "\n";
        return Response<void>(false, "Failed to DELETE key: " + errOpt->error);
    }

    // std::cout << "[DbImpl] Successfully deleted key " << key << "\n";
    return Response<void>(true, "Successfully DELETE key " + key);
}

Response<void> DbImpl::forceCompactForTests()
{
    m_diskManager->compact();
    return Response<void>(true, "Successfully compacted DB");
}

Response<void> DbImpl::forceFlushForTests()
{
    m_memTable->flushToDisk();
    return Response<void>(true, "Successfully flushed memtable to disk");
}