#include <gtest/gtest.h>
#include "core/db.h"
#include "factories/db_factory.h"

#include <filesystem>
#include <iostream>

namespace
{

    void cleanupStorage()
    {
        std::error_code ec;
        std::filesystem::remove_all("./wal", ec);
        std::filesystem::remove_all("./sstables", ec);
    }

    class DbStorageTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            cleanupStorage();
        }

        void TearDown() override
        {
            // cleanupStorage();
        }
    };

} // namespace

TEST_F(DbStorageTest, PutAndGet)
{
    std::unique_ptr<DbImpl> db = DbFactory::createDbForTests(); // dies at the end of the scope

    auto res1 = db->put("hello", "world");
    EXPECT_TRUE(res1.success);

    auto res2 = db->get("hello");
    EXPECT_TRUE(res2.success);
    EXPECT_EQ(res2.data, "world");
}

// TEST #2 (flushes work) : write 3->flush->get
TEST_F(DbStorageTest, FlushWritesToDisk)
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

// TEST #3: Flush keeps latest updates
// this guy fails sometimes
// REASON: b2b flushes can occur in the same millisecond -> both files have the same timestamp.
// SOLUTION 1: record timestamps with higher resolution
// SOLUTION 2: append a monotonically increasing counter, OR read fields in level 0 in LIFO order (easier!)
TEST_F(DbStorageTest, FlushKeepsLatestUpdates)
{
    auto db = DbFactory::createDbForTests();

    ASSERT_TRUE(db->put("key", "old").success);
    EXPECT_TRUE(db->forceFlushForTests().success);

    ASSERT_TRUE(db->put("key", "new").success);
    EXPECT_TRUE(db->forceFlushForTests().success);

    auto result = db->get("key");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data, "new");
}

TEST_F(DbStorageTest, FlushRespectsDeletes)
{
    auto db = DbFactory::createDbForTests();

    ASSERT_TRUE(db->put("ghost", "value").success);
    EXPECT_TRUE(db->forceFlushForTests().success);

    ASSERT_TRUE(db->del("ghost").success);
    EXPECT_TRUE(db->forceFlushForTests().success);

    auto result = db->get("ghost");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.data.has_value());
}

// TEST #3 (flushes respect updates): write -> flush -> write -> flush -> get

// TEST #4 (flushes respect deletes): write -> flush -> del -> flush -> get

// Tests for compaction should focus on what happens to keys that are ON DISK.
// That's why everytime I modify a value, I flush it.
// If I want soemthing to appear with the tombstone value on disk, I write the key -> flush -> delete key -> flush.

TEST_F(DbStorageTest, CompactionPreservesEntries)
{
    auto db = DbFactory::createDbForTests();

    db->put("a", "1");
    db->put("b", "2");
    db->put("c", "3");
    EXPECT_TRUE(db->forceFlushForTests().success);

    db->put("d", "4");
    db->put("e", "5");
    db->put("f", "6");
    EXPECT_TRUE(db->forceFlushForTests().success);

    EXPECT_TRUE(db->forceCompactForTests().success);

    EXPECT_EQ(db->get("a").data, "1");
    EXPECT_EQ(db->get("c").data, "3");
    EXPECT_EQ(db->get("e").data, "5");
}

TEST_F(DbStorageTest, CompactionKeepsLatestOverwrite)
{
    auto db = DbFactory::createDbForTests();

    ASSERT_TRUE(db->put("key", "old").success);
    EXPECT_TRUE(db->forceFlushForTests().success);

    ASSERT_TRUE(db->put("key", "new").success);
    EXPECT_TRUE(db->forceFlushForTests().success);

    EXPECT_TRUE(db->forceCompactForTests().success);

    auto result = db->get("key");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data, "new");
}

TEST_F(DbStorageTest, CompactionRespectsDeletes)
{
    auto db = DbFactory::createDbForTests();

    ASSERT_TRUE(db->put("ghost", "value").success);
    EXPECT_TRUE(db->forceFlushForTests().success);

    ASSERT_TRUE(db->del("ghost").success);
    EXPECT_TRUE(db->forceFlushForTests().success);

    EXPECT_TRUE(db->forceCompactForTests().success);

    auto result = db->get("ghost");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.data.has_value());
}
