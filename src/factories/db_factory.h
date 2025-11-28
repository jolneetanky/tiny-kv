#ifndef DB_FACTORY_H
#define DB_FACTORY_H

#include <memory>
#include <string>
#include "core/db.h"
#include "core/db_impl.h"

struct DbFactoryConfig
{
    int memtableCapacity = 3;
    uint64_t walId = 0;
    std::string walDirectory = "./wal";
    std::string sstableDirectory = "./sstables";
};

class DbFactory
{
public:
    static std::unique_ptr<DB> createDefaultDb();
    static std::unique_ptr<DbImpl> createDbForTests();
    static std::unique_ptr<DbImpl> createDbWithConfig(const DbFactoryConfig &config);
};

#endif
