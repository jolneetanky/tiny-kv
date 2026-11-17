#include <gtest/gtest.h>
#include "core/db.h"
#include "core/sstable_manager/block_based/block_based_table_format.h"
#include "core/sstable_manager/in_memory_table_format.h"
#include "core/sstable_manager/sstable_reader.h"
#include "core/sstable_manager/sstable.h"
#include "core/storage_manager/storage_manager_impl.h"
#include "factories/db_factory.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <vector>

namespace
{
    class VectorIterator final : public tinykv::Iterator
    {
    public:
        explicit VectorIterator(std::vector<Entry> entries) : m_entries{std::move(entries)}, m_index{0} {}

        bool Valid() const override
        {
            return m_index < m_entries.size();
        }

        void SeekToFirst() override
        {
            m_index = 0;
        }

        void SeekToLast() override
        {
            m_index = m_entries.empty() ? 0 : m_entries.size() - 1;
        }

        void Seek(const std::string &key) override
        {
            m_index = 0;
            while (m_index < m_entries.size() && m_entries[m_index].key < key)
            {
                ++m_index;
            }
        }

        void Next() override
        {
            ++m_index;
        }

        const std::string &Key() const override
        {
            return m_entries[m_index].key;
        }

        const std::string &Value() const override
        {
            return m_entries[m_index].val;
        }

        bool isTombstone() const override
        {
            return m_entries[m_index].tombstone;
        }

    private:
        std::vector<Entry> m_entries;
        std::size_t m_index;
    };

    void cleanupStorage()
    {
        std::error_code ec;
        std::filesystem::remove_all("./wal", ec);
        std::filesystem::remove_all("./sstables", ec);
    }

    std::size_t countRegularFiles(const std::filesystem::path &directory)
    {
        std::size_t count = 0;
        for (const auto &entry : std::filesystem::directory_iterator{directory})
        {
            if (entry.is_regular_file())
                ++count;
        }
        return count;
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
            cleanupStorage();
        }
    };

} // namespace

TEST_F(DbStorageTest, CompactionConditionStartsAtFourLevelZeroTables)
{
    SystemContext systemContext;
    StorageManagerImpl storageManager(
        systemContext,
        "./sstables",
        3,
        std::make_unique<InMemoryTableFormat>());

    EXPECT_FALSE(storageManager.isCompactionNeeded());

    for (int table = 0; table < 4; ++table)
    {
        VectorIterator entries{std::vector<Entry>{Entry{"key-" + std::to_string(table), "value"}}};
        ASSERT_FALSE(storageManager.write(entries, 0));

        EXPECT_EQ(storageManager.isCompactionNeeded(), table == 3);
    }
}

TEST_F(DbStorageTest, LevelZeroAgeOrderAndFileNumbersSurviveRestart)
{
    auto writeValue = [](StorageManagerImpl &storageManager, const std::string &value)
    {
        VectorIterator entries{std::vector<Entry>{Entry{"key", value}}};
        return storageManager.write(entries, 0);
    };

    SystemContext firstContext;
    StorageManagerImpl firstStorageManager(
        firstContext,
        "./sstables",
        3,
        std::make_unique<InMemoryTableFormat>());

    ASSERT_FALSE(writeValue(firstStorageManager, "old"));
    ASSERT_FALSE(writeValue(firstStorageManager, "new"));

    SystemContext reopenedContext;
    StorageManagerImpl reopenedStorageManager(
        reopenedContext,
        "./sstables",
        3,
        std::make_unique<InMemoryTableFormat>());
    ASSERT_FALSE(reopenedStorageManager.initLevels());

    auto reopenedValue = reopenedStorageManager.get("key");
    ASSERT_TRUE(reopenedValue.has_value());
    EXPECT_EQ(reopenedValue->val, "new");

    ASSERT_FALSE(writeValue(reopenedStorageManager, "newest"));

    std::set<FileNumber> fileNumbers;
    SSTableReader reader;
    for (const auto &entry : std::filesystem::directory_iterator{"./sstables/level-0"})
    {
        if (entry.is_regular_file())
        {
            SSTable table = reader.read(entry.path().string());
            fileNumbers.insert(table.meta().m_file_number);
        }
    }

    EXPECT_EQ(fileNumbers, (std::set<FileNumber>{0, 1, 2}));

    SystemContext secondReopenContext;
    StorageManagerImpl secondReopenStorageManager(
        secondReopenContext,
        "./sstables",
        3,
        std::make_unique<InMemoryTableFormat>());
    ASSERT_FALSE(secondReopenStorageManager.initLevels());

    auto newestValue = secondReopenStorageManager.get("key");
    ASSERT_TRUE(newestValue.has_value());
    EXPECT_EQ(newestValue->val, "newest");
}

TEST_F(DbStorageTest, BlockBasedTableReaderIteratesAcrossDataBlocks)
{
    BlockBasedTableFormat format;
    std::vector<Entry> tableEntries;
    for (int i = 0; i < 12; ++i)
    {
        const std::string key = std::string{"key-"} + (i < 10 ? "0" : "") + std::to_string(i);
        tableEntries.emplace_back(key, std::string(1000, static_cast<char>('a' + i)), i == 7);
    }

    VectorIterator entries{std::move(tableEntries)};
    const auto reader = format.writeTable(
        "./sstables/level-0/table-iterator-test", entries, 1234, 99);
    std::unique_ptr<tinykv::Iterator> iterator = reader->NewSortedIterator();

    ASSERT_TRUE(iterator->Valid());
    EXPECT_EQ(iterator->Key(), "key-00");
    for (int i = 0; i < 12; ++i)
    {
        const std::string expectedKey = std::string{"key-"} + (i < 10 ? "0" : "") + std::to_string(i);
        ASSERT_TRUE(iterator->Valid());
        EXPECT_EQ(iterator->Key(), expectedKey);
        EXPECT_EQ(iterator->isTombstone(), i == 7);
        if (i != 7)
            EXPECT_EQ(iterator->Value(), std::string(1000, static_cast<char>('a' + i)));

        if (i == 9)
        {
            iterator->Next();
            ASSERT_TRUE(iterator->Valid());
            EXPECT_EQ(iterator->Key(), "key-10");
        }
        else
        {
            iterator->Next();
        }
    }
    EXPECT_FALSE(iterator->Valid());

    iterator->Seek("key-10");
    ASSERT_TRUE(iterator->Valid());
    EXPECT_EQ(iterator->Key(), "key-10");
    iterator->Next();
    ASSERT_TRUE(iterator->Valid());
    EXPECT_EQ(iterator->Key(), "key-11");
    iterator->Next();
    EXPECT_FALSE(iterator->Valid());

    iterator->Seek("key-12");
    EXPECT_FALSE(iterator->Valid());
    iterator->SeekToLast();
    ASSERT_TRUE(iterator->Valid());
    EXPECT_EQ(iterator->Key(), "key-11");
}

TEST_F(DbStorageTest, BlockBasedTableReaderReadsMetadataBlock)
{
    BlockBasedTableFormat format;
    VectorIterator entries{
        std::vector<Entry>{
            Entry{"apple", "1"},
            Entry{"banana", "2"},
            Entry{"zebra", "3"},
        },
    };

    std::shared_ptr<const TableReader> reader = format.writeTable("./sstables/level-0/table-metadata-test", entries, 1234, 99);
    SSTableMetadata meta = reader->meta();

    EXPECT_EQ(meta.m_file_number, 99);
    EXPECT_EQ(meta.m_timestamp, 1234);
    EXPECT_EQ(meta.m_min_key, "apple");
    EXPECT_EQ(meta.m_max_key, "zebra");

    EXPECT_FALSE(reader->withinRange("aardvark"));
    EXPECT_TRUE(reader->withinRange("apple"));
    EXPECT_TRUE(reader->withinRange("banana"));
    EXPECT_TRUE(reader->withinRange("zebra"));
    EXPECT_FALSE(reader->withinRange("zzzz"));

}

// bloom filter test
// TEST(SSTableTest, WithinRangeUsesInclusiveStartAndEndKeys)
// {
//     std::vector<Entry> entries{
//         Entry{"b", "2"},
//         Entry{"d", "4"},
//         Entry{"f", "6"},
//     };
//     SSTable table(SSTableMetadata{0, 0, "b", "f"}, std::move(entries));

//     EXPECT_FALSE(table.withinRange("a"));
//     EXPECT_TRUE(table.withinRange("b"));
//     EXPECT_TRUE(table.withinRange("d"));
//     EXPECT_TRUE(table.withinRange("f"));
//     EXPECT_FALSE(table.withinRange("g"));
// }

// TEST 1: general put and get (in-memory, not yet flushed)
TEST_F(DbStorageTest, PutAndGet)
{
    // std::unique_ptr<DbImpl> db = DbFactory::createDbForTests(); // dies at the end of the scope
    std::unique_ptr<DbImpl> db = DbFactory::createBlockBasedDbForTests();

    auto res1 = db->put("hello", "world");
    EXPECT_TRUE(res1.ok);

    auto res2 = db->get("hello");
    EXPECT_TRUE(res2.ok);
    EXPECT_EQ(res2.data, "world");
}

// TEST #2 (flushes work) : write 3->flush->get
TEST_F(DbStorageTest, FlushWritesToDisk)
{
    // auto db = DbFactory::createDbForTests();
    std::unique_ptr<DbImpl> db = DbFactory::createBlockBasedDbForTests();

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
    // auto db = DbFactory::createDbForTests();
    std::unique_ptr<DbImpl> db = DbFactory::createBlockBasedDbForTests();

    ASSERT_TRUE(db->put("key", "old").ok);
    EXPECT_TRUE(db->forceFlushForTests().ok);

    ASSERT_TRUE(db->put("key", "new").ok);
    EXPECT_TRUE(db->forceFlushForTests().ok);

    auto result = db->get("key");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.data, "new");
}

// TEST #4 - flush respects delectes
// put -> flush
// del -> flush
// get -> expect not to find the key, as it has been deleted
TEST_F(DbStorageTest, FlushRespectsDeletes)
{
    // auto db = DbFactory::createDbForTests();
    std::unique_ptr<DbImpl> db = DbFactory::createBlockBasedDbForTests();

    ASSERT_TRUE(db->put("ghost", "value").ok);
    EXPECT_TRUE(db->forceFlushForTests().ok);

    ASSERT_TRUE(db->del("ghost").ok);
    EXPECT_TRUE(db->forceFlushForTests().ok);

    auto result = db->get("ghost");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.data.has_value());
}

TEST_F(DbStorageTest, TombstoneInNewerLevelHidesOlderValue)
{
    std::filesystem::create_directories("./sstables/level-0");
    std::filesystem::create_directories("./sstables/level-1");

    BlockBasedTableFormat tableFormat;
    VectorIterator olderValue{std::vector<Entry>{Entry{"key", "old"}}};
    VectorIterator tombstone{std::vector<Entry>{Entry{"key", "", true}}};

    tableFormat.writeTable("./sstables/level-1/table-older", olderValue, 100, 1);
    tableFormat.writeTable("./sstables/level-0/table-tombstone", tombstone, 200, 2);

    SystemContext systemContext;
    StorageManagerImpl storageManager(
        systemContext,
        "./sstables",
        3,
        std::make_unique<BlockBasedTableFormat>());
    ASSERT_FALSE(storageManager.initLevels());

    EXPECT_FALSE(storageManager.get("key").has_value());
}

// Tests for compaction should focus on what happens to keys that are ON DISK.
// That's why everytime I modify a value, I flush it.
// If I want soemthing to appear with the tombstone value on disk, I write the key -> flush -> delete key -> flush.

// TEST #5: compaction preserves entries
// honestly should test with overwriting keys
TEST_F(DbStorageTest, CompactionPreservesEntries)
{
    // auto db = DbFactory::createDbForTests();
    std::unique_ptr<DbImpl> db = DbFactory::createBlockBasedDbForTests();

    db->put("b", "1");
    db->put("a", "2");
    db->put("c", "3");
    EXPECT_TRUE(db->forceFlushForTests().ok);

    db->put("f", "4");
    db->put("e", "5");
    db->put("g", "6");
    EXPECT_TRUE(db->forceFlushForTests().ok);

    EXPECT_TRUE(db->forceCompactForTests().ok);

    EXPECT_EQ(db->get("b").data, "1");
    EXPECT_EQ(db->get("c").data, "3");
    EXPECT_EQ(db->get("e").data, "5");
}

// TEST #6 - compaction keeps the latest overwrite
// Because both keys are are they same, they lie in the same key range
// So this test makes sure that the latest overwrite is always reflected.
TEST_F(DbStorageTest, CompactionKeepsLatestOverwrite)
{
    // auto db = DbFactory::createDbForTests();
    std::unique_ptr<DbImpl> db = DbFactory::createBlockBasedDbForTests();

    ASSERT_TRUE(db->put("key", "old").ok);
    EXPECT_TRUE(db->forceFlushForTests().ok);

    ASSERT_TRUE(db->put("key", "new").ok);
    EXPECT_TRUE(db->forceFlushForTests().ok);

    EXPECT_TRUE(db->forceCompactForTests().ok);

    auto result = db->get("key");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.data, "new");
}

// TEST #7 - compaction respects deletes
// put->flush
// del->flush
// compact
// get(key), the key should not be found.
TEST_F(DbStorageTest, CompactionRespectsDeletes)
{
    // auto db = DbFactory::createDbForTests();
    std::unique_ptr<DbImpl> db = DbFactory::createBlockBasedDbForTests();

    ASSERT_TRUE(db->put("ghost", "value").ok);
    EXPECT_TRUE(db->forceFlushForTests().ok);

    ASSERT_TRUE(db->del("ghost").ok);
    EXPECT_TRUE(db->forceFlushForTests().ok);

    EXPECT_TRUE(db->forceCompactForTests().ok);

    auto result = db->get("ghost");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.data.has_value());
}

TEST_F(DbStorageTest, CompactionMergesL0IntoNonEmptyL1)
{
    DbFactoryConfig config;
    config.tableFormat = TableFormatKind::BlockBased;
    config.maxLevels = 1; // Keep the first compaction output in L1.
    std::unique_ptr<DbImpl> db = DbFactory::createDbWithConfig(config);

    ASSERT_TRUE(db->put("a", "old-a").ok);
    ASSERT_TRUE(db->put("c", "old-c").ok);
    ASSERT_TRUE(db->forceFlushForTests().ok);
    ASSERT_TRUE(db->forceCompactForTests().ok);

    ASSERT_EQ(countRegularFiles("./sstables/level-0"), 0);
    ASSERT_EQ(countRegularFiles("./sstables/level-1"), 1);

    // This L0 table overlaps the existing L1 table at key "c".
    ASSERT_TRUE(db->put("c", "new-c").ok);
    ASSERT_TRUE(db->put("e", "new-e").ok);
    ASSERT_TRUE(db->forceFlushForTests().ok);
    ASSERT_EQ(countRegularFiles("./sstables/level-0"), 1);
    ASSERT_EQ(countRegularFiles("./sstables/level-1"), 1);

    ASSERT_TRUE(db->forceCompactForTests().ok);

    EXPECT_EQ(countRegularFiles("./sstables/level-0"), 0);
    EXPECT_EQ(countRegularFiles("./sstables/level-1"), 1);

    auto oldOnlyKey = db->get("a");
    ASSERT_TRUE(oldOnlyKey.ok);
    EXPECT_EQ(oldOnlyKey.data, "old-a");

    auto overwrittenKey = db->get("c");
    ASSERT_TRUE(overwrittenKey.ok);
    EXPECT_EQ(overwrittenKey.data, "new-c");

    auto newOnlyKey = db->get("e");
    ASSERT_TRUE(newOnlyKey.ok);
    EXPECT_EQ(newOnlyKey.data, "new-e");
}

// TODO: Write test for SSTableReader.write()
// TODO: Write test for SSTableReader.read()
// Reader should read what was just written
// Serialize it to a proper SSTable file
// And we should be able to iterate through and list all entries that were written.
