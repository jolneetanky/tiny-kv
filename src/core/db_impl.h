#ifndef DB_IMPL_H
#define DB_IMPL_H

#include "db.h"
#include "../disk_manager/disk_manager.h"
#include "../mem_table/mem_table.h"

class DbImpl : public DB
{
private:
    MemTable &m_memtable;

public:
    DbImpl(MemTable &memTable);
    void put(std::string key, std::string val) override;
    std::string get(std::string key) const override;
    void del(std::string key) override;
};

#endif