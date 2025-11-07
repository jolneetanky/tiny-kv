// This class implements the DB interface.
// TODO: implement the DB interface.
#include "db_impl.h"
#include <iostream>
#include <optional>
#include "../types/entry.h"

DbImpl::DbImpl(MemTable &memTable, SSTableManager &ssTableManager) : m_memtable{memTable}, m_ssTableManager{ssTableManager} {};

Response<void> DbImpl::put(std::string key, std::string val)
{
    std::optional<Error> errOpt{m_memtable.put(key, val)};

    if (errOpt)
    {
        std::cout << "[DbImpl] error: " << errOpt->error << "\n";
        return Response<void>(false, errOpt->error);
    }

    std::cout << "[DbImpl]" << " PUT " << key << ", " << val << "\n";
    return Response<void>(true, "Successfully PUT key " + key + " in DB");
}

Response<std::string> DbImpl::get(std::string key) const
{
    std::optional<Entry> optEntry{m_memtable.get(key)};

    if (optEntry && optEntry->tombstone)
    {
        std::cout << "[DbImpl]" << " Key \"" << key << "\" does not exist." << "\n";
        return Response<std::string>(false, "Key does not exist", std::nullopt);
    }

    if (!optEntry)
    {
        optEntry = m_ssTableManager.get(key);

        if (!optEntry)
        {
            std::cout << "[DbImpl]" << " Key \"" << key << "\" does not exist." << "\n";
            return Response<std::string>(false, "Key does not exist", std::nullopt);
        }
    }

    std::cout << "[DbImpl] GOT: " << optEntry.value().val << "\n";
    return Response<std::string>(true, "", optEntry.value().val);
}

Response<void> DbImpl::del(std::string key)
{
    std::optional<Error> errOpt{m_memtable.del(key)};

    if (errOpt)
    {
        std::cout << "[DbImpl.del] Failed to DELETE key: " << errOpt->error << "\n";
        return Response<void>(false, "Failed to DELETE key: " + errOpt->error);
    }

    std::cout << "[DbImpl] Successfully deleted key " << key << "\n";
    return Response<void>(true, "Successfully DELETE key " + key);
}