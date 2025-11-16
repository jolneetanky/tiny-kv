#ifndef DB_FACTORY_H
#define DB_FACTORY_H

#include <memory>
#include "core/db.h"
#include "core/db_impl.h"

class DbFactory
{
public:
    static std::unique_ptr<DB> createDefaultDb();
    static std::unique_ptr<DbImpl> createDbForTests();
};

#endif