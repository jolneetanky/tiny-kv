#ifndef DB_IMPL_H
#define DB_IMPL_H

#include "db.h"

// DbImpl inherits from the virtual class DB
class DbImpl : public DB
{
private:
    std::map<std::string, std::string> m_kvStore; // cannot be const because we modify this data structure.

public:
    std::string put(std::string key, std::string val);
    std::string get(std::string key);
    std::string del(std::string key);
};

#endif