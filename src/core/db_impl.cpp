// This class implements the DB interface.
// TODO: implement the DB interface.
#include "db_impl.h"
#include <iostream>
#include <optional>

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
    std::string val{m_memtable.get(key).val};
    if (val.empty())  {
        std::cout << "[DbImpl]" << " Key \"" << key << "\" does not exist." << "\n";
    } else {
        std::cout << "[DbImpl] GOT: " << val << "\n";
    }

    return val;
}

void DbImpl::del(std::string key)
{
    m_memtable.del(key);
    std::cout << "[DbImpl] Successfully deleted key " << key << "\n";
}