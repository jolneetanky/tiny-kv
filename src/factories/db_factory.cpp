#include "db_factory.h"
#include "contexts/system_context.h"
#include "core/storage_manager/storage_manager_impl.h"
#include "core/skip_list/skip_list_impl.h"
#include "core/wal/wal.h"
#include "core/mem_table/mem_table_impl.h"
#include "core/db_impl.h"

std::unique_ptr<DbImpl> DbFactory::createDbWithConfig(const DbFactoryConfig &config)
{
    auto systemCtx = std::make_unique<SystemContext>();
    auto storageManagerImpl = std::make_unique<StorageManagerImpl>(*systemCtx, config.sstableDirectory, config.maxLevels);
    auto skipListImpl = std::make_unique<SkipListImpl>();
    auto wal = std::make_unique<WAL>(config.walId, config.walDirectory);
    auto memTableImpl = std::make_unique<MemTableImpl>(config.memtableCapacity, *skipListImpl, *storageManagerImpl, *wal, *systemCtx);

    return std::make_unique<DbImpl>(
        std::move(systemCtx),
        std::move(skipListImpl),
        std::move(wal),
        std::move(storageManagerImpl),
        std::move(memTableImpl));
}

std::unique_ptr<DB> DbFactory::createDefaultDb()
{
    auto dbImpl = createDbWithConfig(DbFactoryConfig{});
    return std::unique_ptr<DB>(std::move(dbImpl));
}

std::unique_ptr<DbImpl> DbFactory::createDbForTests()
{
    return createDbWithConfig(DbFactoryConfig{});
}
