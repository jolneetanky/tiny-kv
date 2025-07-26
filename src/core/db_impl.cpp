// This class implements the DB interface.
// TODO: implement the DB interface.
#include "db_impl.h"
#include <iostream>
#include <optional>
#include "../types/entry.h"

DbImpl::DbImpl(MemTable &memTable, SSTableManager &ssTableManager) : m_memtable{memTable}, m_ssTableManager{ssTableManager} {};

void DbImpl::put(std::string key, std::string val)
{
    std::optional<Error> errOpt {m_memtable.put(key, val)};

    if (errOpt) {
        std::cout << "[DbImpl] error: " << errOpt->error << "\n";
        return;
    }

    std::cout << "[DbImpl]" << " PUT " << key << ", " << val << "\n";
}

std::string DbImpl::get(std::string key) const
{
    std::optional<Entry> optEntry {m_memtable.get(key)};

    if (!optEntry) {
        optEntry = m_ssTableManager.get(key);

        if (!optEntry) {
            std::cout << "[DbImpl]" << " Key \"" << key << "\" does not exist in memtable." << "\n";
            return "";
        }
    }

    std::cout << "[DbImpl] GOT: " << optEntry.value().val << "\n";
    return optEntry.value().val;
}

void DbImpl::del(std::string key)
{
    std::optional<Error> errOpt { m_memtable.del(key) };

    if (errOpt) {
        std::cout << "[DbImpl.del] Failed to DELETE key: " << errOpt->error << "\n";
        return;
    }

    std::cout << "[DbImpl] Successfully deleted key " << key << "\n";
}