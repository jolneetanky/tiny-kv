// This class implements the DB interface.
// TODO: implement the DB interface.
#include "db_impl.h"
#include <iostream>

// DbImpl::DbImpl(WriteBuffer &writeBuffer)
// {
//     m_writeBuffer{writeBuffer};
// }

// member initializer list (before constructor body executes)
DbImpl::DbImpl(WriteBuffer &writeBuffer, DiskManager &diskManager) : m_writeBuffer(writeBuffer), m_diskManager(diskManager) {};

void DbImpl::put(std::string key, std::string val)
{
    m_writeBuffer.put(key, val);

    std::cout << "[DbImpl]" << " PUT " << key << ", " << val << "\n";
}

std::string DbImpl::get(std::string key) const
{
    std::string val{m_writeBuffer.get(key)};

    // If not in buffer, search from disk
    if (val.empty())
    {
        val = m_diskManager.getKey(key);
    }

    if (val.empty())
    {
        std::cout << "[DbImpl]" << " Key \"" << key << "\" does not exist." << "\n";
    }
    else
    {
        std::cout << "[DbImpl]" << " GOT: " << val << "\n";
    }
    return val;
}

void DbImpl::del(std::string key)
{
    m_writeBuffer.del(key);
    std::cout << "[DbImpl] Successfully deleted key " << key << "\n";
}