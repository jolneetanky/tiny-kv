#include "db_factory.h"
#include "contexts/system_context.h"
#include "core/sstable_manager/sstable_manager_impl.h"
#include "core/skip_list/skip_list_impl.h"
#include "core/wal/wal.h"
#include "core/mem_table/mem_table_impl.h"
#include "core/db_impl.h"

std::unique_ptr<DB> DbFactory::createDefaultDb()
{

    // initialize factories
    // auto bff = std::make_unique<DefaultBloomFilterFactory>();

    // pass in system context
    auto systemCtx = std::make_unique<SystemContext>();

    // initialize classes
    auto ssTableManagerImpl = std::make_unique<SSTableManagerImpl>(*systemCtx);
    auto skipListImpl = std::make_unique<SkipListImpl>();
    auto wal = std::make_unique<WAL>(0);
    auto memTableImpl = std::make_unique<MemTableImpl>(3, *skipListImpl, *ssTableManagerImpl, *wal, *systemCtx);

    return std::make_unique<DbImpl>(
        std::move(systemCtx),
        std::move(skipListImpl),
        std::move(wal),
        std::move(ssTableManagerImpl),
        std::move(memTableImpl));
}

std::unique_ptr<DbImpl> DbFactory::createDbForTests()
{
    // initialize factories
    // auto bff = std::make_unique<DefaultBloomFilterFactory>(); // dies when this goes out of scope; system, also we don't need this guy

    // pass in system context
    auto systemCtx = std::make_unique<SystemContext>();

    // initialize classes
    auto ssTableManagerImpl = std::make_unique<SSTableManagerImpl>(*systemCtx);
    auto skipListImpl = std::make_unique<SkipListImpl>();
    auto wal = std::make_unique<WAL>(0);
    auto memTableImpl = std::make_unique<MemTableImpl>(3, *skipListImpl, *ssTableManagerImpl, *wal, *systemCtx);

    return std::make_unique<DbImpl>(
        std::move(systemCtx),
        std::move(skipListImpl),
        std::move(wal),
        std::move(ssTableManagerImpl),
        std::move(memTableImpl));
}