#include <iostream>
#include <fstream>
#include <memory>
#include <chrono>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <cmath>

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include "factories/db_factory.h"

namespace
{
    constexpr int kMemtableCapacity = 1024;
    constexpr int kRandomStep = 104729; // prime step to spread reads across the key space
    constexpr int kReadOps = 100000;

    struct TestPaths
    {
        std::filesystem::path root;
        std::filesystem::path wal;
        std::filesystem::path sstables;
    };

    uint64_t getCurrentRSSBytes()
    {
#if defined(__APPLE__)
        mach_task_basic_info info;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
        {
            return 0;
        }
        return static_cast<uint64_t>(info.resident_size);
#elif defined(__linux__)
        std::ifstream statm("/proc/self/statm");
        uint64_t totalPages = 0;
        uint64_t residentPages = 0;
        statm >> totalPages >> residentPages;
        return residentPages * static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
#else
        return 0;
#endif
    }

    // Get current RSS in MB. This is intentionally not ru_maxrss, which is peak RSS.
    double getRSSMB()
    {
        return static_cast<double>(getCurrentRSSBytes()) / (1024.0 * 1024.0);
    }

    std::string makeKey(int index)
    {
        std::string n = std::to_string(index);
        return "key-" + std::string(12 - std::min<int>(12, n.size()), '0') + n;
    }

    std::string makeValue(int valueSize, int seed)
    {
        return std::string(valueSize, static_cast<char>('a' + (seed % 26)));
    }

    TestPaths makePaths(const char *name)
    {
        const auto root = std::filesystem::path("bench/run-data/memory-investigation") / name;
        return TestPaths{root, root / "wal", root / "sstables"};
    }

    void resetPaths(const TestPaths &paths)
    {
        std::error_code ec;
        std::filesystem::remove_all(paths.root, ec);
        std::filesystem::create_directories(paths.wal, ec);
        std::filesystem::create_directories(paths.sstables, ec);
    }

    void cleanupPaths(const TestPaths &paths)
    {
        std::error_code ec;
        std::filesystem::remove_all(paths.root, ec);
    }

    DbFactoryConfig makeConfig(const TestPaths &paths)
    {
        DbFactoryConfig config;
        config.memtableCapacity = kMemtableCapacity;
        config.walDirectory = paths.wal.string();
        config.sstableDirectory = paths.sstables.string();
        config.walId = 0;
        return config;
    }

    uint64_t directorySizeBytes(const std::filesystem::path &dir)
    {
        uint64_t totalSize = 0;
        std::error_code ec;
        for (auto &p : std::filesystem::recursive_directory_iterator(dir, ec))
        {
            if (!ec && std::filesystem::is_regular_file(p.path(), ec))
            {
                totalSize += std::filesystem::file_size(p.path(), ec);
            }
        }
        return totalSize;
    }

    int fileCount(const std::filesystem::path &dir)
    {
        int count = 0;
        std::error_code ec;
        for (auto &p : std::filesystem::recursive_directory_iterator(dir, ec))
        {
            if (!ec && std::filesystem::is_regular_file(p.path(), ec))
            {
                count++;
            }
        }
        return count;
    }

    class MemorySampler
    {
    public:
        void start()
        {
            peakRSSBytes.store(getCurrentRSSBytes());
        }

        void update()
        {
            uint64_t current = getCurrentRSSBytes();
            uint64_t prevPeak = peakRSSBytes.load();
            while (current > prevPeak && !peakRSSBytes.compare_exchange_weak(prevPeak, current))
            {
                // loop until peakRSSBytes updated if current > prevPeak
            }
        }

        double stopAndGetPeakMB()
        {
            update();
            return static_cast<double>(peakRSSBytes.load()) / (1024.0 * 1024.0);
        }

        double currentPeakMB() const
        {
            return static_cast<double>(peakRSSBytes.load()) / (1024.0 * 1024.0);
        }

    private:
        std::atomic<uint64_t> peakRSSBytes{0};
    };

    void investigateMemoryUsage()
    {
        std::ofstream csv("bench/memory_investigation_results.csv");
        csv << "keys,log2_keys,key_size_bytes,value_size_bytes,current_rss_before_reads_mb,current_rss_after_reads_mb,peak_rss_mb,disk_size_bytes,sstable_files,read_ops,warm_read_latency_us\n";

        // Intermediate dataset sizes help determine whether lookup slowdown
        // is gradual (memory locality degradation) or exhibits a performance cliff.
        std::vector<int> datasetSizes = {
            1024,
            4096,
            16384,
            65536,
            250000,
            500000,
            1000000,
            2000000,
            4000000,
            8000000,
            10000000};
        int valueSize = 64;
        int keySize = static_cast<int>(makeKey(0).size());

        for (int numKeys : datasetSizes)
        {
            std::string testName = "warm-read-scaling-" + std::to_string(numKeys);
            const auto paths = makePaths(testName.c_str());
            resetPaths(paths);

            std::cout << "\n=== Warm read scaling test with " << numKeys << " keys ===" << std::endl;

            MemorySampler sampler;
            sampler.start();

            auto db = DbFactory::createDbWithConfig(makeConfig(paths));

            std::cout << "Loading " << numKeys << " keys..." << std::endl;
            for (int i = 0; i < numKeys; ++i)
            {
                auto response = db->put(makeKey(i), makeValue(valueSize, i));
                if (!response.ok)
                {
                    std::cerr << "Failed to load data at key " << i << std::endl;
                    return;
                }

                if (i % 10000 == 0)
                {
                    sampler.update();
                }
            }
            sampler.update();

            std::cout << "Flushing to SSTables..." << std::endl;
            auto flushResp = db->forceFlushForTests();
            if (!flushResp.ok)
            {
                std::cerr << "Failed to flush to disk" << std::endl;
                return;
            }
            sampler.update();

            const double currentRSSBeforeReadMB = getRSSMB();
            const double peakRSSBeforeReadMB = sampler.currentPeakMB();
            const uint64_t diskBytes = directorySizeBytes(paths.root);
            const int sstableFiles = fileCount(paths.sstables);
            const double log2Keys = std::log2(static_cast<double>(numKeys));

            std::cout << "Current RSS before reads: " << currentRSSBeforeReadMB << " MB" << std::endl;
            std::cout << "Peak RSS before reads: " << peakRSSBeforeReadMB << " MB" << std::endl;
            std::cout << "Disk bytes: " << diskBytes << std::endl;
            std::cout << "SSTable files: " << sstableFiles << std::endl;
            std::cout << "Running " << kReadOps << " warm reads..." << std::endl;

            int64_t cursor = 0;
            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < kReadOps; ++i)
            {
                cursor = (cursor + kRandomStep) % numKeys;
                auto response = db->get(makeKey(static_cast<int>(cursor)));
                if (!response.ok)
                {
                    std::cerr << "Failed to read key during warm read phase. Cursor: " << cursor
                              << ", key: " << makeKey(static_cast<int>(cursor))
                              << ", message: " << response.message << std::endl;
                    return;
                }

                if (i % 1000 == 0)
                {
                    sampler.update();
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            sampler.update();

            const double warmLatencyUs = std::chrono::duration<double, std::micro>(end - start).count() / static_cast<double>(kReadOps);
            const double currentRSSAfterReadMB = getRSSMB();
            const double peakRSSAfterReadMB = sampler.stopAndGetPeakMB();

            std::cout << "Warm read latency: " << warmLatencyUs << " us/op" << std::endl;
            std::cout << "Current RSS after reads: " << currentRSSAfterReadMB << " MB" << std::endl;
            std::cout << "Peak RSS during run: " << peakRSSAfterReadMB << " MB" << std::endl;

            csv << numKeys << ","
                << log2Keys << ","
                << keySize << ","
                << valueSize << ","
                << currentRSSBeforeReadMB << ","
                << currentRSSAfterReadMB << ","
                << peakRSSAfterReadMB << ","
                << diskBytes << ","
                << sstableFiles << ","
                << kReadOps << ","
                << warmLatencyUs << "\n";

            db.reset();
            cleanupPaths(paths);
        }

        csv.close();
        std::cout << "\n=== Warm read scaling investigation complete ===" << std::endl;
        std::cout << "Results written to: bench/memory_investigation_results.csv" << std::endl;
    }

} // namespace

int main()
{
    try
    {
        investigateMemoryUsage();
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }
}
