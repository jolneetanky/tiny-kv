#ifndef DB_IMPL_H
#define DB_IMPL_H

#include "db.h"
#include "../writebuffer/writebuffer.h"
#include "../disk_manager/disk_manager.h"
class DbImpl : public DB
{
private:
    WriteBuffer &m_writeBuffer;
    DiskManager &m_diskManager;

public:
    DbImpl(WriteBuffer &writeBuffer, DiskManager &diskManager); // constructor
    void put(std::string key, std::string val) override;
    std::string get(std::string key) const override;
    void del(std::string key) override;
};

#endif