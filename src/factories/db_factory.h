#ifndef DB_FACTORY_H
#define DB_FACTORY_H

#include <memory>
#include <string>
#include "core/db.h"
#include "core/db_impl.h"

enum class TableFormatKind
{
    InMemory,
    BlockBased,
};

// TODO: make this a json config
struct DbFactoryConfig
{
    int memtableCapacity = 3;
    uint64_t walId = 0;
    std::string walDirectory = "./wal";
    std::string sstableDirectory = "./sstables";
    int maxLevels = 3;
    TableFormatKind tableFormat = TableFormatKind::InMemory;
};

// make the methods static so we don't need to create an instance of `DbFactory`
class DbFactory
{
public:
    // general factory function
    static std::unique_ptr<DbImpl> createDbWithConfig(const DbFactoryConfig &config);

    // factory functions to create in-memory based DB
    // exposes a DB, ie. the main interface
    static std::unique_ptr<DB> createDefaultDb();
    // exposes a DbImpl, which comes with additional functions for testing (eg. DbImpl::compact(), etc.)
    static std::unique_ptr<DbImpl> createDbForTests();

    // factory functions to create block-based DB
    static std::unique_ptr<DB> createBlockBasedDb();
    static std::unique_ptr<DbImpl> createBlockBasedDbForTests();
};

#endif
