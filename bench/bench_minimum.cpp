#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#include "factories/db_factory.h"

namespace
{
    constexpr int kMemtableCapacity = 1024;
    constexpr int kRandomStep = 97;

    struct BenchPaths
    {
        std::filesystem::path root;
        std::filesystem::path wal;
        std::filesystem::path sstables;
    };

    std::string makeKey(int index)
    {
        std::string n = std::to_string(index);
        return "key-" + std::string(12 - std::min<int>(12, n.size()), '0') + n;
    }

    std::string makeMissingKey(int index)
    {
        std::string n = std::to_string(index);
        return "missing-" + std::string(8 - std::min<int>(8, n.size()), '0') + n;
    }

    std::string makeValue(int valueSize, int seed)
    {
        return std::string(valueSize, static_cast<char>('a' + (seed % 26)));
    }

    BenchPaths makePaths(const char *name, const benchmark::State &state)
    {
        const auto root = std::filesystem::path("bench/run-data/minimum") /
                          (std::string(name) + "-" + std::to_string(state.range(0)) + "-" +
                           std::to_string(state.range(1)));
        return BenchPaths{root, root / "wal", root / "sstables"};
    }

    void resetPaths(const BenchPaths &paths)
    {
        std::error_code ec;
        std::filesystem::remove_all(paths.root, ec);
        std::filesystem::create_directories(paths.wal, ec);
        std::filesystem::create_directories(paths.sstables, ec);
    }

    void cleanupPaths(const BenchPaths &paths)
    {
        std::error_code ec;
        std::filesystem::remove_all(paths.root, ec);
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

    void requireOk(const protocol::Response &response, const char *message)
    {
        if (!response.ok)
        {
            throw std::runtime_error(message);
        }
    }

    void preloadDataset(DB &db, int keyCount, int valueSize)
    {
        for (int i = 0; i < keyCount; ++i)
        {
            requireOk(db.put(makeKey(i), makeValue(valueSize, i)), "failed to preload benchmark dataset");
        }
        requireOk(db.forceFlushForTests(), "failed to flush benchmark dataset");
    }

    void setCommonCounters(benchmark::State &state, int keyCount, int valueSize)
    {
        state.counters["keys"] = static_cast<double>(keyCount);
        state.counters["value_bytes"] = static_cast<double>(valueSize);
    }

    void registerWriteBytes(benchmark::State &state, int64_t operations, int valueSize)
    {
        state.SetItemsProcessed(operations);
        state.SetBytesProcessed(operations * static_cast<int64_t>(makeKey(0).size() + valueSize));
    }

    void BM_PutSequential(benchmark::State &state)
    {
        const int keyCount = static_cast<int>(state.range(0));
        const int valueSize = static_cast<int>(state.range(1));
        const auto paths = makePaths("put-sequential", state);

        for (auto _ : state)
        {
            state.PauseTiming();
            resetPaths(paths);
            auto db = DbFactory::createDbWithConfig(makeConfig(paths));
            state.ResumeTiming();

            for (int i = 0; i < keyCount; ++i)
            {
                auto response = db->put(makeKey(i), makeValue(valueSize, i));
                if (!response.ok)
                {
                    state.SkipWithError("PUT failed during sequential write benchmark");
                    break;
                }
            }

            state.PauseTiming();
            db.reset();
            state.ResumeTiming();
        }

        cleanupPaths(paths);
        registerWriteBytes(state, state.iterations() * keyCount, valueSize);
        setCommonCounters(state, keyCount, valueSize);
    }

    void BM_PutRandom(benchmark::State &state)
    {
        const int keyCount = static_cast<int>(state.range(0));
        const int valueSize = static_cast<int>(state.range(1));
        const auto paths = makePaths("put-random", state);

        for (auto _ : state)
        {
            state.PauseTiming();
            resetPaths(paths);
            auto db = DbFactory::createDbWithConfig(makeConfig(paths));
            state.ResumeTiming();

            int cursor = 0;
            for (int i = 0; i < keyCount; ++i)
            {
                cursor = (cursor + kRandomStep) % keyCount;
                auto response = db->put(makeKey(cursor), makeValue(valueSize, cursor));
                if (!response.ok)
                {
                    state.SkipWithError("PUT failed during random write benchmark");
                    break;
                }
            }

            state.PauseTiming();
            db.reset();
            state.ResumeTiming();
        }

        cleanupPaths(paths);
        registerWriteBytes(state, state.iterations() * keyCount, valueSize);
        setCommonCounters(state, keyCount, valueSize);
    }

    void BM_GetExistingRandom(benchmark::State &state)
    {
        const int keyCount = static_cast<int>(state.range(0));
        const int valueSize = static_cast<int>(state.range(1));
        const auto paths = makePaths("get-existing-random", state);
        resetPaths(paths);

        auto db = DbFactory::createDbWithConfig(makeConfig(paths));
        preloadDataset(*db, keyCount, valueSize);

        int cursor = 0;
        int64_t bytesRead = 0;
        for (auto _ : state)
        {
            cursor = (cursor + kRandomStep) % keyCount;
            auto response = db->get(makeKey(cursor));
            if (!response.ok || !response.data.has_value())
            {
                state.SkipWithError("GET hit failed during benchmark");
                break;
            }
            bytesRead += static_cast<int64_t>(response.data->size());
            benchmark::DoNotOptimize(response);
        }

        db.reset();
        cleanupPaths(paths);
        state.SetItemsProcessed(state.iterations());
        state.SetBytesProcessed(bytesRead);
        setCommonCounters(state, keyCount, valueSize);
    }

    void BM_GetMissingRandom(benchmark::State &state)
    {
        const int keyCount = static_cast<int>(state.range(0));
        const int valueSize = static_cast<int>(state.range(1));
        const auto paths = makePaths("get-missing-random", state);
        resetPaths(paths);

        auto db = DbFactory::createDbWithConfig(makeConfig(paths));
        preloadDataset(*db, keyCount, valueSize);

        int cursor = 0;
        for (auto _ : state)
        {
            cursor = (cursor + kRandomStep) % keyCount;
            auto response = db->get(makeMissingKey(cursor));
            if (response.ok || response.data.has_value())
            {
                state.SkipWithError("GET miss unexpectedly found a key");
                break;
            }
            benchmark::DoNotOptimize(response);
        }

        db.reset();
        cleanupPaths(paths);
        state.SetItemsProcessed(state.iterations());
        setCommonCounters(state, keyCount, valueSize);
    }

    void BM_OverwriteRandom(benchmark::State &state)
    {
        const int keyCount = static_cast<int>(state.range(0));
        const int valueSize = static_cast<int>(state.range(1));
        const auto paths = makePaths("overwrite-random", state);
        resetPaths(paths);

        auto db = DbFactory::createDbWithConfig(makeConfig(paths));
        preloadDataset(*db, keyCount, valueSize);

        int cursor = 0;
        for (auto _ : state)
        {
            cursor = (cursor + kRandomStep) % keyCount;
            auto response = db->put(makeKey(cursor), makeValue(valueSize, cursor + 1));
            if (!response.ok)
            {
                state.SkipWithError("PUT failed during overwrite benchmark");
                break;
            }
            benchmark::DoNotOptimize(response);
        }

        db.reset();
        cleanupPaths(paths);
        registerWriteBytes(state, state.iterations(), valueSize);
        setCommonCounters(state, keyCount, valueSize);
    }

    void runMixed(benchmark::State &state, int writeEveryN)
    {
        const int keyCount = static_cast<int>(state.range(0));
        const int valueSize = static_cast<int>(state.range(1));
        const auto paths = makePaths(writeEveryN == 20 ? "mixed-95r-5w" : "mixed-50r-50w", state);
        resetPaths(paths);

        auto db = DbFactory::createDbWithConfig(makeConfig(paths));
        preloadDataset(*db, keyCount, valueSize);

        int cursor = 0;
        int64_t op = 0;
        int64_t bytesProcessed = 0;
        int64_t writes = 0;
        for (auto _ : state)
        {
            cursor = (cursor + kRandomStep) % keyCount;
            if (op % writeEveryN == 0)
            {
                auto response = db->put(makeKey(cursor), makeValue(valueSize, cursor + 2));
                if (!response.ok)
                {
                    state.SkipWithError("PUT failed during mixed benchmark");
                    break;
                }
                ++writes;
                bytesProcessed += static_cast<int64_t>(makeKey(cursor).size() + valueSize);
                benchmark::DoNotOptimize(response);
            }
            else
            {
                auto response = db->get(makeKey(cursor));
                if (!response.ok || !response.data.has_value())
                {
                    state.SkipWithError("GET failed during mixed benchmark");
                    break;
                }
                bytesProcessed += static_cast<int64_t>(response.data->size());
                benchmark::DoNotOptimize(response);
            }
            ++op;
        }

        db.reset();
        cleanupPaths(paths);
        state.SetItemsProcessed(state.iterations());
        state.SetBytesProcessed(bytesProcessed);
        state.counters["writes"] = static_cast<double>(writes);
        setCommonCounters(state, keyCount, valueSize);
    }

    void BM_Mixed95Read5Write(benchmark::State &state)
    {
        runMixed(state, 20);
    }

    void BM_Mixed50Read50Write(benchmark::State &state)
    {
        runMixed(state, 2);
    }

    void applyDefaultArgs(benchmark::internal::Benchmark *bench)
    {
        bench->Args({1024, 64})
            ->Args({4096, 64})
            ->Args({4096, 256})
            ->Unit(benchmark::kMicrosecond);
    }

    BENCHMARK(BM_PutSequential)->Apply(applyDefaultArgs);
    BENCHMARK(BM_PutRandom)->Apply(applyDefaultArgs);
    BENCHMARK(BM_GetExistingRandom)->Apply(applyDefaultArgs);
    BENCHMARK(BM_GetMissingRandom)->Apply(applyDefaultArgs);
    BENCHMARK(BM_OverwriteRandom)->Apply(applyDefaultArgs);
    BENCHMARK(BM_Mixed95Read5Write)->Apply(applyDefaultArgs);
    BENCHMARK(BM_Mixed50Read50Write)->Apply(applyDefaultArgs);
} // namespace
