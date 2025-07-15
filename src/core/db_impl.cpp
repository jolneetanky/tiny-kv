// This class implements the DB interface.
// TODO: implement the DB interface.
#include "db_impl.h"
#include <iostream>

// DbImpl::DbImpl(WriteBuffer &writeBuffer)
// {
//     m_writeBuffer{writeBuffer};
// }

// member initializer list (before constructor body executes)
DbImpl::DbImpl(WriteBuffer &writeBuffer) : m_writeBuffer(writeBuffer) {};

std::string DbImpl::put(std::string key, std::string val)
{
    m_writeBuffer.put(key, val);

    std::cout << "[DbImpl]" << " PUT " << key << ", " << val << "\n";
    return key;
}

std::string DbImpl::get(std::string key)
{
    // std::string val = {m_kvStore[key]};
    std::string val{m_writeBuffer.get(key)};

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

std::string DbImpl::del(std::string key)
{

    // std::cout << "[DbImpl]" << " DELETED: (" << key << ", " << m_kvStore[key] << ")" << "\n";
    // std::string val{m_kvStore[key]};
    // m_kvStore.erase(key);
    // return val;
    std::string k{m_writeBuffer.del(key)};
    // std::cout << "[DbImpl]" << " DELETED: " << key << "\n";
    return k;
}