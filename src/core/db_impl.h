#ifndef DB_IMPL_H
#define DB_IMPL_H

#include "db.h"
#include "../writebuffer/writebuffer.h"
class DbImpl : public DB
{
private:
    WriteBuffer &m_writeBuffer;

public:
    DbImpl(WriteBuffer &writeBuffer); // constructor
    std::string put(std::string key, std::string val);
    std::string get(std::string key);
    std::string del(std::string key);
};

#endif