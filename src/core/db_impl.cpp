// This class implements the DB interface.
// TODO: implement the DB interface.
#include "db_impl.h"
#include <iostream>

std::string DbImpl::put(std::string key, std::string val)
{
    m_kvStore[key] = val;
    std::cout << "PUT " << key << ", " << val << "\n";
    return key;
}

std::string DbImpl::get(std::string key)
{
    std::cout << "GOT: " << m_kvStore[key] << "\n";
    return m_kvStore[key];
}

std::string DbImpl::del(std::string key)
{
    std::cout << "DELETED: (" << key << ", " << m_kvStore[key] << ")" << "\n";
    std::string val{m_kvStore[key]};
    m_kvStore.erase(key);
    return val;
}