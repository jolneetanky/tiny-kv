#include <gtest/gtest.h>
#include "core/db.h"
#include "factories/db_factory.h"

#include <iostream>

TEST(DbTest, PutAndGet)
{
    std::unique_ptr<DbImpl> db = DbFactory::createDbForTests(); // dies at the end of the scope

    auto res1 = db->put("hello", "world");
    EXPECT_TRUE(res1.success);

    auto res2 = db->get("hello");
    EXPECT_TRUE(res2.success);
    EXPECT_EQ(res2.data, "world");
}

// TEST #2(flushes work) : write 3->flush->get
TEST(DbTest, FlushWritesToDisk)
{
    auto db = DbFactory::createDbForTests();

    db->put("a", "1");
    db->put("b", "2");
    db->put("c", "3");

    db->forceFlushForTests();

    EXPECT_EQ(db->get("a").data, "1");
    EXPECT_EQ(db->get("b").data, "2");
    EXPECT_EQ(db->get("c").data, "3");
}

// TEST (compaction works): write 3 -> flush -> write 3 -> flush -> compact -> get

// TEST (compaction overrides older entries)
// write "a, v1" -> flush -> write "a, v2" -> flush -> compact -> get "a"

// TEST: write -> flush -> delete -> flush -> get

// TEST: write -> flush -> delete -> flush -> compact -> get