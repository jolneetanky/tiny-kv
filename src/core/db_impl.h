#ifndef DB_IMPL_H
#define DB_IMPL_H

#include "db.h"
#include "../mem_table/mem_table.h"
#include "../sstable_manager/sstable_manager.h"

class DbImpl : public DB
{
private:
    MemTable &m_memtable;
    SSTableManager &m_ssTableManager;

public:
    DbImpl(MemTable &memTable, SSTableManager &ssTableManager);
    void put(std::string key, std::string val) override;
    std::string get(std::string key) const override;
    void del(std::string key) override;
};

#endif