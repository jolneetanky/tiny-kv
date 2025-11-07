#ifndef DB_IMPL_H
#define DB_IMPL_H

#include "db.h"
#include "../mem_table/mem_table.h"
#include "../sstable_manager/sstable_manager.h"

// interface class
class DbImpl : public DB
{
private:
    MemTable &m_memtable;             // memory component
    SSTableManager &m_ssTableManager; // disk component

public:
    DbImpl(MemTable &memTable, SSTableManager &ssTableManager);
    Response<void> put(std::string key, std::string val) override;
    Response<std::string> get(std::string key) const override;
    Response<void> del(std::string key) override;
};

#endif