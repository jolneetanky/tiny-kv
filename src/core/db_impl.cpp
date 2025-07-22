// This class implements the DB interface.
// TODO: implement the DB interface.
#include "db_impl.h"
#include <iostream>
#include <optional>
#include "../types/entry.h"

DbImpl::DbImpl(MemTable &memTable) : m_memtable{memTable} {};

void DbImpl::put(std::string key, std::string val)
{
    Error* err {m_memtable.put(key, val)};

    if (err != nullptr) {
        std::cout << "[DbImpl] error: " << err->error << "\n";
    }

    std::cout << "[DbImpl]" << " PUT " << key << ", " << val << "\n";
}

std::string DbImpl::get(std::string key) const
{
    // std::string val{m_memtable.get(key).val};
    const Entry* ptr {m_memtable.get(key)};
    if (ptr == nullptr)  {
        std::cout << "[DbImpl]" << " Key \"" << key << "\" does not exist in memtable." << "\n";
        return "";
    } else {
        std::cout << "[DbImpl] GOT: " << (*ptr).val << "\n";
    }

    return (*ptr).val;
}

void DbImpl::del(std::string key)
{
    m_memtable.del(key);
    std::cout << "[DbImpl] Successfully deleted key " << key << "\n";
}