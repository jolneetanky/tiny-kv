#include <gtest/gtest.h>

#include "contexts/system_context.h"
#include "core/db_impl.h"
#include "core/mem_table/mem_table_impl.h"
#include "core/skip_list/skip_list_impl.h"
#include "core/storage_manager/storage_manager.h"
#include "core/wal/wal.h"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>

namespace
{
    struct CompactionState
    {
        std::mutex mutex;
        std::condition_variable cv;
        bool needed = true;
        bool clearConditionOnCompact = true;
        int compactionCount = 0;
    };

    class CountingStorageManager final : public StorageManager
    {
    public:
        explicit CountingStorageManager(std::shared_ptr<CompactionState> state)
            : m_state{std::move(state)}
        {
        }

        std::optional<Error> write(tinykv::Iterator &, int) override
        {
            return std::nullopt;
        }

        std::optional<Entry> get(const std::string &) const override
        {
            return std::nullopt;
        }

        bool isCompactionNeeded() const override
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->needed;
        }

        std::optional<Error> compact() override
        {
            {
                // mock: compact() clears the state so that next
                // the next `isCompactionNeeded()` returns false
                std::lock_guard<std::mutex> lock(m_state->mutex);
                ++m_state->compactionCount;
                if (m_state->clearConditionOnCompact)
                    m_state->needed = false;
            }
            m_state->cv.notify_all();
            return std::nullopt;
        }

    private:
        std::shared_ptr<CompactionState> m_state;
    };

    bool waitForCompactionCount(const std::shared_ptr<CompactionState> &state, int expected)
    {
        std::unique_lock<std::mutex> lock(state->mutex);
        return state->cv.wait_for(lock, std::chrono::seconds(2), [&]
                                  { return state->compactionCount >= expected; });
    }

    std::unique_ptr<DbImpl> makeDb(const std::shared_ptr<CompactionState> &state)
    {
        auto systemContext = std::make_unique<SystemContext>();
        auto skipList = std::make_unique<SkipListImpl>();
        auto wal = std::make_unique<WAL>(991, "./test-data/compaction-worker/wal");
        auto storageManager = std::make_unique<CountingStorageManager>(state);
        auto memTable = std::make_unique<MemTableImpl>(32, *skipList, *storageManager, *wal, *systemContext);

        return std::make_unique<DbImpl>(
            std::move(systemContext),
            std::move(skipList),
            std::move(wal),
            std::move(storageManager),
            std::move(memTable));
    }

    class CompactionWorkerTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            std::error_code ec;
            std::filesystem::remove_all("./test-data/compaction-worker", ec);
        }

        void TearDown() override
        {
            std::error_code ec;
            std::filesystem::remove_all("./test-data/compaction-worker", ec);
        }
    };
}

// TESTS:
// 1. the worker wakes up for compaction
TEST_F(CompactionWorkerTest, WakesForCompactionAndCanBeScheduledAgainAfterConditionClears)
{
    auto state = std::make_shared<CompactionState>();
    auto db = makeDb(state);

    ASSERT_TRUE(db->put("first", "value").ok);
    ASSERT_TRUE(waitForCompactionCount(state, 1));

    // set compaction `needed` to `true` to trigger the compaction worker
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->needed = true;
    }

    ASSERT_TRUE(db->put("second", "value").ok);
    EXPECT_TRUE(waitForCompactionCount(state, 2));
}

TEST_F(CompactionWorkerTest, NoOpCompactionCanBeRequestedAgainAfterThePreviousCallReturns)
{
    auto state = std::make_shared<CompactionState>();
    state->clearConditionOnCompact = false;
    auto db = makeDb(state);

    ASSERT_TRUE(db->put("first", "value").ok);
    ASSERT_TRUE(waitForCompactionCount(state, 1));

    ASSERT_TRUE(db->put("second", "value").ok);
    EXPECT_TRUE(waitForCompactionCount(state, 2));
}
