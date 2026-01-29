#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <sys/resource.h>
#endif

#include "factories/db_factory.h"
#include "core/db_impl.h"

namespace fs = std::filesystem;

// -------------------- Disk size --------------------
static uint64_t dir_size_bytes(const fs::path &p)
{
    uint64_t total = 0;
    if (!fs::exists(p))
        return 0;
    for (auto const &entry : fs::recursive_directory_iterator(p))
    {
        if (entry.is_regular_file())
        {
            std::error_code ec;
            total += static_cast<uint64_t>(fs::file_size(entry.path(), ec));
        }
    }
    return total;
}

static void rm_rf(const fs::path &p)
{
    std::error_code ec;
    if (fs::exists(p))
        fs::remove_all(p, ec);
    fs::create_directories(p, ec);
}

// -------------------- RSS (current) --------------------
static uint64_t current_rss_bytes()
{
#if defined(__APPLE__)
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) != KERN_SUCCESS)
    {
        return 0;
    }
    return static_cast<uint64_t>(info.resident_size);
#elif defined(__linux__)
    std::ifstream in("/proc/self/status");
    std::string line;
    while (std::getline(in, line))
    {
        if (line.rfind("VmRSS:", 0) == 0)
        {
            // VmRSS: <kb> kB
            std::string key, unit;
            uint64_t kb = 0;
            std::istringstream iss(line);
            iss >> key >> kb >> unit;
            return kb * 1024ULL;
        }
    }
    return 0;
#else
    return 0;
#endif
}

// Optional “true” peak on Linux via getrusage (in addition to sampling)
static uint64_t peak_rss_bytes_linux_best_effort()
{
#if defined(__linux__)
    rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) == 0)
    {
        return static_cast<uint64_t>(ru.ru_maxrss) * 1024ULL; // KB -> bytes
    }
#endif
    return 0;
}

// Samples RSS in a background thread while compaction runs.
struct RssSampler
{
    std::atomic<bool> run{false};
    std::atomic<uint64_t> peak{0};
    std::thread th;

    void start(std::chrono::milliseconds period = std::chrono::milliseconds(2))
    {
        run.store(true, std::memory_order_release);
        peak.store(current_rss_bytes(), std::memory_order_release);
        th = std::thread([this, period]()
                         {
      while (run.load(std::memory_order_acquire)) {
        uint64_t rss = current_rss_bytes();
        uint64_t prev = peak.load(std::memory_order_relaxed);
        while (rss > prev && !peak.compare_exchange_weak(prev, rss, std::memory_order_relaxed)) {
          // keep trying
        }
        std::this_thread::sleep_for(period);
      } });
    }

    void stop()
    {
        run.store(false, std::memory_order_release);
        if (th.joinable())
            th.join();
        // Linux: also consider ru_maxrss
        uint64_t linux_peak = peak_rss_bytes_linux_best_effort();
        if (linux_peak > peak.load())
            peak.store(linux_peak);
    }
};

// -------------------- Workload generation --------------------
static std::string make_key(uint64_t i, int width = 16)
{
    // fixed width keeps lexicographic == numeric ordering (helps SSTable ranges)
    std::string s = "k";
    s.reserve(width);
    auto num = std::to_string(i);
    s.append(std::max(0, width - 1 - (int)num.size()), '0');
    s.append(num);
    return s;
}

static std::string make_value(uint64_t i, size_t value_size)
{
    std::string v(value_size, 'x');
    for (int b = 0; b < 8 && b < (int)value_size; b++)
    {
        v[b] = char('a' + ((i >> (b * 3)) & 15));
    }
    return v;
}

// Creates lots of SSTables by flushing frequently.
// Also optionally creates overwrites / tombstones to stress merge logic.
static void load_workload(DbImpl &db,
                          uint64_t num_entries,
                          size_t value_size,
                          int flush_every,
                          bool include_overwrites,
                          bool include_deletes)
{
    for (uint64_t i = 0; i < num_entries; i++)
    {
        db.put(make_key(i), make_value(i, value_size));
        if (flush_every > 0 && ((int)((i + 1) % (uint64_t)flush_every) == 0))
        {
            db.forceFlushForTests();
        }
    }
    db.forceFlushForTests();

    if (include_overwrites)
    {
        // overwrite ~10% of keys (new values)
        for (uint64_t i = 0; i < num_entries; i += 10)
        {
            db.put(make_key(i), make_value(i + 1234567, value_size));
            if (flush_every > 0 && ((int)((i / 10 + 1) % (uint64_t)flush_every) == 0))
            {
                db.forceFlushForTests();
            }
        }
        db.forceFlushForTests();
    }

    if (include_deletes)
    {
        // delete ~10% of keys (tombstones)
        for (uint64_t i = 5; i < num_entries; i += 10)
        {
            db.del(make_key(i));
            if (flush_every > 0 && ((int)((i / 10 + 1) % (uint64_t)flush_every) == 0))
            {
                db.forceFlushForTests();
            }
        }
        db.forceFlushForTests();
    }
}

// -------------------- DB factory helper --------------------
static std::unique_ptr<DbImpl> make_db(const std::string &wal_dir,
                                       const std::string &sstable_dir,
                                       int memtable_capacity,
                                       int max_levels)
{
    DbFactoryConfig cfg;
    cfg.memtableCapacity = memtable_capacity;
    cfg.maxLevels = max_levels;
    cfg.walDirectory = wal_dir;
    cfg.sstableDirectory = sstable_dir;
    cfg.walId = 0;
    return DbFactory::createDbWithConfig(cfg);
}

// -------------------- Bench fixture --------------------
class CompactionBench : public benchmark::Fixture
{
public:
    // You can tweak these in BENCHMARK_DEFINE_F below by reading state.range(i).
    static constexpr const char *kWalDir = "./bench_wal";
    static constexpr const char *kSstDir = "./bench_sstables";

    void SetUp(const ::benchmark::State &) override
    {
        rm_rf(kWalDir);
        rm_rf(kSstDir);
    }

    void TearDown(const ::benchmark::State &) override
    {
        // Keep artifacts only if you want to inspect; otherwise delete.
        std::error_code ec;
        fs::remove_all(kWalDir, ec);
        fs::remove_all(kSstDir, ec);
    }
};

// Measures only compaction time (setup & data load are paused).
// Params via ranges:
//  range(0) = num_entries
//  range(1) = value_size
//  range(2) = memtable_capacity
//  range(3) = max_levels
BENCHMARK_DEFINE_F(CompactionBench, CompactOnly)(benchmark::State &state)
{
    const uint64_t num_entries = static_cast<uint64_t>(state.range(0));
    const size_t value_size = static_cast<size_t>(state.range(1));
    const int memtable_capacity = static_cast<int>(state.range(2));
    const int max_levels = static_cast<int>(state.range(3));

    for (auto _ : state)
    {
        state.PauseTiming();

        // fresh DB each iteration to keep compaction work comparable
        rm_rf(kWalDir);
        rm_rf(kSstDir);
        auto db = make_db(kWalDir, kSstDir, memtable_capacity, max_levels);

        // flush frequently to make many SSTables / compaction work
        int flush_every = std::max(1, memtable_capacity);
        load_workload(*db, num_entries, value_size, flush_every,
                      /*include_overwrites=*/true,
                      /*include_deletes=*/true);

        const uint64_t disk_before = dir_size_bytes(kSstDir) + dir_size_bytes(kWalDir);

        // Start RSS sampler *before* timing, but don’t include start/stop in timing.
        RssSampler sampler;
        sampler.start(std::chrono::milliseconds(2));

        state.ResumeTiming();

        auto res = db->forceCompactForTests();

        state.PauseTiming();

        sampler.stop();

        const uint64_t disk_after = dir_size_bytes(kSstDir) + dir_size_bytes(kWalDir);
        const uint64_t peak_rss = sampler.peak.load();

        if (!res.success)
        {
            state.SkipWithError("forceCompactForTests() failed");
        }

        // Report extra metrics (not part of the official “time per iteration”).
        state.counters["entries"] = static_cast<double>(num_entries);
        state.counters["value_bytes"] = static_cast<double>(value_size);
        state.counters["memtable_cap"] = static_cast<double>(memtable_capacity);
        state.counters["max_levels"] = static_cast<double>(max_levels);

        state.counters["disk_before_mb"] = benchmark::Counter(
            (double)disk_before / (1024.0 * 1024.0), benchmark::Counter::kDefaults);
        state.counters["disk_after_mb"] = benchmark::Counter(
            (double)disk_after / (1024.0 * 1024.0), benchmark::Counter::kDefaults);

        state.counters["peak_rss_mb"] = benchmark::Counter(
            (double)peak_rss / (1024.0 * 1024.0), benchmark::Counter::kDefaults);
    }
}

static void Args_CompactOnly(benchmark::internal::Benchmark *b)
{
    // Pick a few realistic scales (tune as needed).
    // num_entries, value_size, memtable_capacity, max_levels
    b->Args({200000, 64, 128, 3});
    b->Args({500000, 256, 256, 4});
    b->Args({1000000, 512, 512, 5});
    b->Args({2000000, 1024, 512, 6});
    // For “large SSTable files”, push value_size and entries higher.
    b->Args({5000000, 2048, 512, 6});
}

BENCHMARK_REGISTER_F(CompactionBench, CompactOnly)
    ->Apply(Args_CompactOnly)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(5)  // more stable
    ->Repetitions(3) // distribution
    ->ReportAggregatesOnly(true);

BENCHMARK_MAIN();
