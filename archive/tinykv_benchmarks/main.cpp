#include <algorithm>
#include <chrono>
#include <filesystem>
#include <future>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "factories/db_factory.h"

namespace
{
struct BenchmarkOptions
{
    size_t numKeys = 10000;
    size_t valueSize = 128;
    size_t parallelism = 1;
    int memtableCapacity = 4096;
};

enum class ScenarioType
{
    PutAscending,
    PutRandom,
    ReadSequential,
    ReadRandom,
};

struct BenchmarkSpec
{
    size_t id = 0;
    ScenarioType type;
    BenchmarkOptions options;
    std::string name;
    uint64_t seed = 0;
};

struct StoragePaths
{
    std::filesystem::path root;
    std::filesystem::path wal;
    std::filesystem::path sstables;
};

struct BenchmarkMetrics
{
    std::string scenario;
    size_t operations = 0;
    size_t bytesProcessed = 0;
    double durationSeconds = 0.0;
    double opsPerSecond = 0.0;
    double mbPerSecond = 0.0;
};

struct BenchmarkResult
{
    BenchmarkSpec spec;
    BenchmarkMetrics metrics;
};

std::string scenarioLabel(ScenarioType type)
{
    switch (type)
    {
    case ScenarioType::PutAscending:
        return "put-ascending";
    case ScenarioType::PutRandom:
        return "put-random";
    case ScenarioType::ReadSequential:
        return "read-sequential";
    case ScenarioType::ReadRandom:
        return "read-random";
    }
    return "unknown";
}

size_t parseSizeT(std::string_view value, std::string_view flag)
{
    try
    {
        size_t idx = 0;
        size_t parsed = std::stoull(std::string(value), &idx);
        if (idx != value.size())
        {
            throw std::invalid_argument("contains trailing characters");
        }
        return parsed;
    }
    catch (const std::exception &ex)
    {
        throw std::invalid_argument("Invalid value for " + std::string(flag) + ": " + std::string(value) + " (" + ex.what() + ")");
    }
}

int parseInt(std::string_view value, std::string_view flag)
{
    try
    {
        size_t idx = 0;
        int parsed = std::stoi(std::string(value), &idx);
        if (idx != value.size())
        {
            throw std::invalid_argument("contains trailing characters");
        }
        return parsed;
    }
    catch (const std::exception &ex)
    {
        throw std::invalid_argument("Invalid value for " + std::string(flag) + ": " + std::string(value) + " (" + ex.what() + ")");
    }
}

BenchmarkOptions parseOptions(int argc, char **argv)
{
    BenchmarkOptions options;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg{argv[i]};
        auto requireValue = [&](std::string_view flag)
        {
            if (i + 1 >= argc)
            {
                throw std::invalid_argument("Missing value for " + std::string(flag));
            }
            return std::string_view{argv[++i]};
        };

        if (arg == "--keys")
        {
            options.numKeys = parseSizeT(requireValue(arg), arg);
        }
        else if (arg == "--value-size")
        {
            options.valueSize = parseSizeT(requireValue(arg), arg);
        }
        else if (arg == "--parallel")
        {
            options.parallelism = parseSizeT(requireValue(arg), arg);
        }
        else if (arg == "--memtable")
        {
            options.memtableCapacity = parseInt(requireValue(arg), arg);
        }
    }

    if (options.numKeys == 0)
    {
        options.numKeys = 1;
    }
    if (options.valueSize == 0)
    {
        options.valueSize = 1;
    }
    if (options.parallelism == 0)
    {
        options.parallelism = 1;
    }
    if (options.memtableCapacity <= 0)
    {
        options.memtableCapacity = 1;
    }

    return options;
}

std::vector<std::string> makeOrderedKeys(size_t count)
{
    std::vector<std::string> keys;
    keys.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        keys.emplace_back("key-" + std::to_string(i));
    }
    return keys;
}

std::vector<std::string> shuffleKeys(std::vector<std::string> keys, uint64_t seed)
{
    std::mt19937_64 gen(seed);
    std::shuffle(keys.begin(), keys.end(), gen);
    return keys;
}

std::string makeValue(size_t bytes)
{
    return std::string(bytes, 'v');
}

size_t computeBytes(const std::vector<std::string> &keys, size_t valueSize)
{
    size_t total = 0;
    for (const auto &key : keys)
    {
        total += key.size();
    }
    total += keys.size() * valueSize;
    return total;
}

StoragePaths prepareStorage(const BenchmarkSpec &spec)
{
    StoragePaths paths;
    paths.root = std::filesystem::path("benchmarks/run-data") / (spec.name + "-" + std::to_string(spec.id));

    std::error_code ec;
    std::filesystem::remove_all(paths.root, ec);
    std::filesystem::create_directories(paths.root);

    paths.wal = paths.root / "wal";
    paths.sstables = paths.root / "sstables";
    std::filesystem::create_directories(paths.wal);
    std::filesystem::create_directories(paths.sstables);
    return paths;
}

void cleanupStorage(const StoragePaths &paths)
{
    std::error_code ec;
    std::filesystem::remove_all(paths.root, ec);
}

std::unique_ptr<DbImpl> createDb(const StoragePaths &paths, const BenchmarkSpec &spec)
{
    DbFactoryConfig config;
    config.memtableCapacity = spec.options.memtableCapacity;
    config.walId = spec.id;
    config.walDirectory = paths.wal.string();
    config.sstableDirectory = paths.sstables.string();
    return DbFactory::createDbWithConfig(config);
}

void ensureSuccess(const Response<void> &resp, std::string_view action)
{
    if (!resp.success)
    {
        throw std::runtime_error(std::string(action) + ": " + resp.message);
    }
}

void ensureSuccess(const Response<std::string> &resp, std::string_view action)
{
    if (!resp.success || !resp.data)
    {
        throw std::runtime_error(std::string(action) + ": " + resp.message);
    }
}

void populate(DbImpl &db, const std::vector<std::string> &keys, const std::string &value)
{
    for (const auto &key : keys)
    {
        ensureSuccess(db.put(key, value), "populate put");
    }
}

BenchmarkMetrics buildMetrics(const BenchmarkSpec &spec, size_t operations, size_t bytes, std::chrono::steady_clock::duration duration)
{
    const double seconds = std::chrono::duration<double>(duration).count();
    BenchmarkMetrics metrics;
    metrics.scenario = spec.name;
    metrics.operations = operations;
    metrics.bytesProcessed = bytes;
    metrics.durationSeconds = seconds;
    metrics.opsPerSecond = seconds > 0.0 ? operations / seconds : 0.0;
    metrics.mbPerSecond = seconds > 0.0 ? (bytes / (1024.0 * 1024.0)) / seconds : 0.0;
    return metrics;
}

BenchmarkResult runPutBenchmark(const BenchmarkSpec &spec, bool randomOrder)
{
    auto orderedKeys = makeOrderedKeys(spec.options.numKeys);
    std::vector<std::string> keys = randomOrder ? shuffleKeys(orderedKeys, spec.seed) : orderedKeys;
    const std::string value = makeValue(spec.options.valueSize);
    const size_t bytes = computeBytes(keys, value.size());

    const auto storage = prepareStorage(spec);
    auto db = createDb(storage, spec);

    auto start = std::chrono::steady_clock::now();
    for (const auto &key : keys)
    {
        ensureSuccess(db->put(key, value), "put");
    }
    auto duration = std::chrono::steady_clock::now() - start;

    cleanupStorage(storage);
    return BenchmarkResult{spec, buildMetrics(spec, keys.size(), bytes, duration)};
}

BenchmarkResult runReadBenchmark(const BenchmarkSpec &spec, bool randomOrder)
{
    auto orderedKeys = makeOrderedKeys(spec.options.numKeys);
    std::vector<std::string> readKeys = randomOrder ? shuffleKeys(orderedKeys, spec.seed) : orderedKeys;
    const std::string value = makeValue(spec.options.valueSize);
    const size_t bytes = computeBytes(readKeys, value.size());

    const auto storage = prepareStorage(spec);
    auto db = createDb(storage, spec);
    populate(*db, orderedKeys, value);
    ensureSuccess(db->forceFlushForTests(), "flush");

    auto start = std::chrono::steady_clock::now();
    for (const auto &key : readKeys)
    {
        auto resp = db->get(key);
        ensureSuccess(resp, "get");
    }
    auto duration = std::chrono::steady_clock::now() - start;

    cleanupStorage(storage);
    return BenchmarkResult{spec, buildMetrics(spec, readKeys.size(), bytes, duration)};
}

BenchmarkResult executeBenchmark(const BenchmarkSpec &spec)
{
    switch (spec.type)
    {
    case ScenarioType::PutAscending:
        return runPutBenchmark(spec, false);
    case ScenarioType::PutRandom:
        return runPutBenchmark(spec, true);
    case ScenarioType::ReadSequential:
        return runReadBenchmark(spec, false);
    case ScenarioType::ReadRandom:
        return runReadBenchmark(spec, true);
    }
    throw std::runtime_error("Unhandled benchmark type");
}

std::vector<BenchmarkSpec> buildSpecs(const BenchmarkOptions &options)
{
    std::vector<BenchmarkSpec> specs;
    specs.reserve(4);
    size_t id = 0;
    for (ScenarioType type : {ScenarioType::PutAscending, ScenarioType::PutRandom, ScenarioType::ReadSequential, ScenarioType::ReadRandom})
    {
        BenchmarkSpec spec;
        spec.id = id++;
        spec.type = type;
        spec.options = options;
        spec.name = scenarioLabel(type);
        spec.seed = 1337u + spec.id;
        specs.push_back(spec);
    }
    return specs;
}

std::vector<BenchmarkResult> runAllBenchmarks(const std::vector<BenchmarkSpec> &specs, size_t parallelism)
{
    if (parallelism <= 1)
    {
        std::vector<BenchmarkResult> results;
        results.reserve(specs.size());
        for (const auto &spec : specs)
        {
            results.push_back(executeBenchmark(spec));
        }
        return results;
    }

    std::vector<BenchmarkResult> results;
    results.reserve(specs.size());

    size_t index = 0;
    while (index < specs.size())
    {
        size_t batch = std::min(parallelism, specs.size() - index);
        std::vector<std::future<BenchmarkResult>> futures;
        futures.reserve(batch);
        for (size_t i = 0; i < batch; ++i)
        {
            futures.emplace_back(std::async(std::launch::async, [spec = specs[index + i]]
                                            { return executeBenchmark(spec); }));
        }

        for (auto &future : futures)
        {
            results.push_back(future.get());
        }

        index += batch;
    }

    return results;
}

void printResults(std::vector<BenchmarkResult> results)
{
    std::sort(results.begin(), results.end(), [](const BenchmarkResult &a, const BenchmarkResult &b)
              { return a.spec.id < b.spec.id; });

    std::cout << "Scenario                Ops        Seconds     Ops/s        MB/s" << "\n";
    for (const auto &result : results)
    {
        const auto &m = result.metrics;
        std::cout << std::left << std::setw(22) << m.scenario
                  << std::right << std::setw(11) << m.operations
                  << std::setw(13) << std::fixed << std::setprecision(4) << m.durationSeconds
                  << std::setw(13) << std::setprecision(2) << m.opsPerSecond
                  << std::setw(12) << std::setprecision(2) << m.mbPerSecond << "\n";
    }
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        const BenchmarkOptions options = parseOptions(argc, argv);
        const auto specs = buildSpecs(options);
        const auto results = runAllBenchmarks(specs, options.parallelism);
        printResults(results);
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Benchmark failed: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
