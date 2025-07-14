#ifndef KVSTORE_H
#define KVSTORE_H

#include <map>
#include <iostream>

// All methods and members of a class are private by default.
class KvStore
{
    std::map<std::string, std::string> m_kvStore; // cannot be const because we modify this data structure.

public:
    std::string put(std::string key, std::string val);

    std::string get(std::string key);

    std::string del(std::string key);
};

#endif