#include "kvstore.h"

std::string KvStore::put(std::string key, std::string val)
{
    m_kvStore[key] = val;
    std::cout << "PUT " << key << ", " << val << "\n";
    return key;
}

std::string KvStore::get(std::string key)
{
    std::cout << "GOT: " << m_kvStore[key] << "\n";
    return m_kvStore[key];
}

std::string KvStore::del(std::string key)
{
    std::cout << "DELETED: (" << key << ", " << m_kvStore[key] << ")" << "\n";
    std::string val{m_kvStore[key]};
    m_kvStore.erase(key);
    return val;
}