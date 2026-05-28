#include <benchmark/benchmark.h>

#include <filesystem>
#include <memory>
#include <string>

#include "factories/db_factory.h"

namespace
{
    constexpr int kMemtableCapacity = 64;

    std::string makeKey(int index)
    {
        return "key-" + std::to_string(index);
    }

    // Creates fixed size values
    std::string makeValue(int valueSize, int seed)
    {
        return std::string(valueSize, static_cast<char>('a' + (seed % 26)));
    }

    struct BenchPaths
    {
        std::filesystem::path root;
        std::filesystem::path wal;
        std::filesystem::path sstables;
    };

    BenchPaths makePaths(const benchmark::State &state)
    {
        const auto root = std::filesystem::path("bench/run-data") /
                          ("read-heavy-" + std::to_string(state.range(0)) + "-" +
                           std::to_string(state.range(1)));
        return BenchPaths{
            root,
            root / "wal",
            root / "sstables",
        };
    }

    void resetPaths(const BenchPaths &paths)
    {
        std::error_code ec;
        std::filesystem::remove_all(paths.root, ec);
        std::filesystem::create_directories(paths.wal, ec);
        std::filesystem::create_directories(paths.sstables, ec);
    }

    DbFactoryConfig makeConfig(const BenchPaths &paths)
    {
        DbFactoryConfig config;
        config.memtableCapacity = kMemtableCapacity;
        config.walDirectory = paths.wal.string();
        config.sstableDirectory = paths.sstables.string();
        config.walId = 0;
        return config;
    }

    void preloadDataset(DB &db, int keyCount, int valueSize)
    {
        for (int i = 0; i < keyCount; ++i)
        {
            benchmark::DoNotOptimize(i);
            auto response = db.put(makeKey(i), makeValue(valueSize, i));
            if (!response.ok)
            {
                throw std::runtime_error("failed to preload benchmark dataset");
            }
        }

        auto flushResponse = db.forceFlushForTests();
        if (!flushResponse.ok)
        {
            throw std::runtime_error("failed to flush benchmark dataset");
        }
    }

    class ReadHeavyFixture : public benchmark::Fixture
    {
    public:
        void SetUp(const ::benchmark::State &state) override
        {
            m_paths = makePaths(state);
            resetPaths(m_paths);

            auto config = makeConfig(m_paths);

            {
                auto preloadDb = DbFactory::createDbWithConfig(config);
                preloadDataset(*preloadDb, static_cast<int>(state.range(0)), static_cast<int>(state.range(1)));
            }

            m_db = DbFactory::createDbWithConfig(config);
        }

        void TearDown(const ::benchmark::State &) override
        {
            m_db.reset();

            std::error_code ec;
            std::filesystem::remove_all(m_paths.root, ec);
        }

    protected:
        BenchPaths m_paths;
        std::unique_ptr<DbImpl> m_db;
    };

    BENCHMARK_DEFINE_F(ReadHeavyFixture, RandomishGets)(benchmark::State &state)
    {
        const int keyCount = static_cast<int>(state.range(0));
        const int valueSize = static_cast<int>(state.range(1));
        int cursor = 0;
        size_t bytesRead = 0;

        // one iteration == one get
        for (auto _ : state)
        {
            cursor = (cursor + 97) % keyCount;
            auto response = m_db->get(makeKey(cursor));

            if (!response.ok || !response.data.has_value())
            {
                state.SkipWithError("GET failed during benchmark");
                break;
            }

            benchmark::DoNotOptimize(response);
            bytesRead += response.data->size();
        }

        state.SetItemsProcessed(state.iterations());
        state.SetBytesProcessed(static_cast<int64_t>(bytesRead));
        state.counters["keys"] = static_cast<double>(keyCount);
        state.counters["value_bytes"] = static_cast<double>(valueSize);
    }

    BENCHMARK_REGISTER_F(ReadHeavyFixture, RandomishGets)
        ->Args({1024, 64})  // 1024 keys, value size 64 (smaller dataset, small values)
        ->Args({4096, 64})  // 4096 keys, value size 64 (larger dataset, small values)
        ->Args({4096, 256}) // 4096 keys, value size 256 (larger dataset, larger value size)
        ->Unit(benchmark::kMicrosecond);
} // namespace
