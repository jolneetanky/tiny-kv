# bench_minimum benchmark results

Generated on 2026-06-13 from the local benchmark run:

```sh
./build-bench/bench/bench_minimum
```

Notes:

- The benchmark reported an inability to determine CPU clock rate from `sysctl` and a thread-affinity warning, but those do not affect the measurements themselves.
- The run was executed on an estimated 10 x 24 MHz CPU configuration in this environment.

## Output

The table below summarizes the main benchmark counters from the run.

| Benchmark                      |  Keys | Value bytes | Time (us) | CPU (us) |  Items/s |   MiB/s |
| ------------------------------ | ----: | ----------: | --------: | -------: | -------: | ------: |
| BM_PutSequential/1024/64       | 1,024 |          64 |    16,447 |   16,159 | 63.3692k | 4.83469 |
| BM_PutSequential/4096/64       | 4,096 |          64 |    64,281 |   62,771 | 65.2533k | 4.97843 |
| BM_PutSequential/4096/256      | 4,096 |         256 |    67,568 |   66,182 | 61.8901k | 16.0543 |
| BM_PutRandom/1024/64           | 1,024 |          64 |    16,462 |   16,212 | 63.1639k | 4.81902 |
| BM_PutRandom/4096/64           | 4,096 |          64 |    64,078 |   62,993 | 65.0229k | 4.96085 |
| BM_PutRandom/4096/256          | 4,096 |         256 |    68,043 |   66,815 | 61.3040k | 15.9022 |
| BM_GetExistingRandom/1024/64   | 1,024 |          64 |     0.190 |    0.189 | 5.28496M | 322.568 |
| BM_GetExistingRandom/4096/64   | 4,096 |          64 |     0.295 |    0.295 | 3.39342M | 207.118 |
| BM_GetExistingRandom/4096/256  | 4,096 |         256 |     0.298 |    0.298 | 3.35450M | 818.971 |
| BM_GetMissingRandom/1024/64    | 1,024 |          64 |     0.089 |    0.089 | 11.2291M |       — |
| BM_GetMissingRandom/4096/64    | 4,096 |          64 |     0.283 |    0.283 | 3.53721M |       — |
| BM_GetMissingRandom/4096/256   | 4,096 |         256 |     0.277 |    0.277 | 3.60843M |       — |
| BM_OverwriteRandom/1024/64     | 1,024 |          64 |      16.6 |     16.2 | 61.5565k | 4.69639 |
| BM_OverwriteRandom/4096/64     | 4,096 |          64 |      17.0 |     16.2 | 61.7610k | 4.71199 |
| BM_OverwriteRandom/4096/256    | 4,096 |         256 |      17.6 |     17.0 | 58.9505k | 15.2917 |
| BM_Mixed95Read5Write/1024/64   | 1,024 |          64 |      1.11 |     1.10 | 908.583k | 56.1487 |
| BM_Mixed95Read5Write/4096/64   | 4,096 |          64 |      1.99 |     1.98 | 504.871k | 31.2001 |
| BM_Mixed95Read5Write/4096/256  | 4,096 |         256 |      2.03 |     2.00 | 499.055k |  122.22 |
| BM_Mixed50Read50Write/1024/64  | 1,024 |          64 |      8.76 |     8.59 | 116.473k | 7.99756 |
| BM_Mixed50Read50Write/4096/64  | 4,096 |          64 |      8.97 |     8.81 | 113.547k | 7.79667 |
| BM_Mixed50Read50Write/4096/256 | 4,096 |         256 |      9.44 |     9.36 | 106.861k | 26.9044 |

> The raw benchmark banner and warnings are omitted here for readability; the table captures the main performance counters from the run.

## Observations

### 1. Reads are blazingly fast

| Benchmark                     |  Keys | Value bytes | Time (us) | CPU (us) |  Items/s |   MiB/s |
| ----------------------------- | ----: | ----------: | --------: | -------: | -------: | ------: |
| BM_GetExistingRandom/1024/64  | 1,024 |          64 |     0.190 |    0.189 | 5.28496M | 322.568 |
| BM_GetExistingRandom/4096/64  | 4,096 |          64 |     0.295 |    0.295 | 3.39342M | 207.118 |
| BM_GetExistingRandom/4096/256 | 4,096 |         256 |     0.298 |    0.298 | 3.35450M | 818.971 |
| BM_GetMissingRandom/1024/64   | 1,024 |          64 |     0.089 |    0.089 | 11.2291M |       — |
| BM_GetMissingRandom/4096/64   | 4,096 |          64 |     0.283 |    0.283 | 3.53721M |       — |
| BM_GetMissingRandom/4096/256  | 4,096 |         256 |     0.277 |    0.277 | 3.60843M |       — |

4096 keys, all flushed to disk initially, yet we have 3M-5M gets per second. Compare this with the write benchmarks and you see that this is much faster, even though theoretically, both workloads touch disk. So what's going on?

Our GET benchmark did:

1. Initialize DB
2. Write to DB
3. Flush all writes to disk
4. Then GET these keys

The reason why reads are still fast is, even after all writes are flushed to disk from the memtable, the SSTables written are kept in memory.

So essentially, our GET benchmark measures warm, in-memory SSTable lookup performance. Hence, every GET performs an in-memory binary search, which is really fast because we don't need to fetch anything from disk at all.

In fact, throughout the lifetime of the program, all SSTables that were ever created, and that were created from previous sessions, are stored in memory.

So TinyKV, even though its supposed to be a storage engine, essentially acts like a cache with no eviction. This sounds like a nighmare for memory management, and I think that's exactly why real production-level key-value stores like RocksDB and LevelDB use a bounded block cache to cap the maximum number of blocks that can reside in RAM at any one time.

**Investigation Result**: See [MEMORY_INVESTIGATION_ANALYSIS.md](MEMORY_INVESTIGATION_ANALYSIS.md) for a detailed memory and latency investigation. TL;DR: The data is not kept in memory across process restarts, but during the benchmark run, SSTable structures are in-memory and searched via binary search, which is very fast.
