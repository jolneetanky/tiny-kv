# Memory Investigation Results

## Summary

This investigation explores the memory usage and performance (in terms of latency) as dataset size scales. It shows how GET performance degrades as dataset size increases and the effects of the memory hierarchy dominate.

## Key Findings

All rows in this run use **16-byte keys** and **64-byte values**. Keys are
generated in the form `key-000000000000`.

### 1. **Memory Usage Scales Linearly with Dataset Size**

|    Dataset Size | Key Size | Value Size | Memory After Load | Memory After Flush |    Delta |
| --------------: | -------: | ---------: | ----------------: | -----------------: | -------: |
|      1,024 keys | 16 bytes |   64 bytes |           1.80 MB |            2.47 MB | +0.67 MB |
|      4,096 keys | 16 bytes |   64 bytes |           2.94 MB |            3.11 MB | +0.17 MB |
|     16,384 keys | 16 bytes |   64 bytes |           4.92 MB |            5.06 MB | +0.14 MB |
|     65,536 keys | 16 bytes |   64 bytes |          11.84 MB |           12.00 MB | +0.16 MB |
|  1,000,000 keys | 16 bytes |   64 bytes |         141.14 MB |          141.19 MB | +0.05 MB |
| 10,000,000 keys | 16 bytes |   64 bytes |         643.12 MB |          643.19 MB | +0.06 MB |

This is expected. After flushing, TinyKV keeps SSTable contents resident in memory, so memory consumption includes both the dataset itself and the in-memory SSTable representation.

### 2. **Warm Read Latency Grows Much Faster Than log₂(n)**

Warm read latencies from the same process after flushing SSTables:

|       Keys | log₂(Keys) |        RSS | Warm Read Latency |
| ---------: | ---------: | ---------: | ----------------: |
|      1,024 |      10.00 |    2.55 MB |          0.294 μs |
|      4,096 |      12.00 |    3.09 MB |          0.349 μs |
|     16,384 |      14.00 |    5.05 MB |          1.076 μs |
|     65,536 |      16.00 |   11.92 MB |          4.516 μs |
|    250,000 |      17.93 |   37.61 MB |         16.389 μs |
|    500,000 |      18.93 |   72.14 MB |         31.777 μs |
|  1,000,000 |      19.93 |  141.31 MB |         74.005 μs |
|  2,000,000 |      20.93 |  279.75 MB |        239.941 μs |
|  4,000,000 |      21.93 |  556.36 MB |        630.640 μs |
|  8,000,000 |      22.93 | 1109.56 MB |       1969.940 μs |
| 10,000,000 |      23.25 | 1386.06 MB |       2748.240 μs |

The key observation is that lookup latency grows far faster than the theoretical growth predicted by binary search.

From **1,000,000** to **10,000,000** keys:

- log₂(n) increases from **19.93** to **23.25** (~17% increase)
- RSS increases from **141 MB** to **1386 MB** (~9.8× increase)
- Warm read latency increases from **74 μs** to **2748 μs** (~37× increase)

If binary-search comparison count were the dominant cost, latency would be expected to increase only modestly. Instead, the results suggest that memory hierarchy effects dominate at larger dataset sizes.

## Revised Hypothesis

The benchmark's fast reads are explained by TinyKV's current SSTable design.

During the benchmark:

1. Data is written to the memtable and flushed into SSTable files.
2. After flush, SSTable contents remain resident in memory.
3. GET operations perform in-memory SSTable lookup using binary search over the in-memory SSTable representation.

As a result, warm reads avoid disk I/O entirely. The excellent lookup performance for smaller datasets comes from operating on memory-resident SSTables rather than repeatedly reading from disk.

However, the benchmark results show that lookup latency grows far faster than binary-search complexity alone would predict. This suggests that once SSTables become sufficiently large, the dominant cost is no longer the number of comparisons performed by binary search. Instead, **memory hierarchy** effects such as cache misses, memory locality, and working-set size become the primary bottlenecks.

## Intermediate Dataset Investigation

To determine whether the slowdown represented a sudden performance cliff or a gradual degradation in locality, additional dataset sizes were benchmarked between 1 million and 10 million keys.

|       Keys |        RSS |    Latency |
| ---------: | ---------: | ---------: |
|    250,000 |   37.61 MB |   16.39 μs |
|    500,000 |   72.14 MB |   31.78 μs |
|  1,000,000 |  141.31 MB |   74.01 μs |
|  2,000,000 |  279.75 MB |  239.94 μs |
|  4,000,000 |  556.36 MB |  630.64 μs |
|  8,000,000 | 1109.56 MB | 1969.94 μs |
| 10,000,000 | 1386.06 MB | 2748.24 μs |

The additional measurements show that performance degradation is gradual rather than cliff-like.

Lookup latency increases steadily as the in-memory SSTable working set grows. This suggests the dominant factor is not a single threshold being crossed, but the cumulative effect of increasingly poor memory locality, larger working sets, and higher cache-miss rates.

This result strengthens the hypothesis that memory hierarchy effects, rather than binary-search complexity, are the primary bottleneck at larger dataset sizes.

## Conclusion

Both benchmarks are measuring warm in-memory SSTable lookup, not disk reads. Because TinyKV keeps SSTable contents resident in memory after flush, GET operations avoid disk access entirely. The investigation shows that while this design provides excellent performance for smaller datasets, lookup latency degrades substantially as the in-memory working set grows and memory hierarchy effects begin to dominate.

6. **The benchmark motivates a block-based SSTable design.**

   The slowdown observed at larger dataset sizes shows why production storage engines do not usually keep full SSTable contents resident in memory. Instead, they keep only compact metadata, indexes, and filters in memory, while loading data blocks on demand through a bounded cache.

## Core Results

1. **Memory usage scales with dataset size.**

   RSS increased from **2.47 MB** after flushing **1,024 keys** to **141.22 MB** after flushing **1,000,000 keys**. This suggests TinyKV’s current SSTable representation keeps a significant amount of data in memory.

2. **Warm read latency increases substantially faster than binary-search complexity predicts.**

   From **1,000,000** to **10,000,000** keys, log₂(n) increased by only **17%**, while measured lookup latency increased by approximately **37×**.

3. **The slowdown appears gradual rather than cliff-like.**

   Additional measurements at **250k**, **500k**, **2M**, **4M**, and **8M** keys show a steady increase in lookup latency as dataset size grows. This suggests that performance degradation is caused by progressively worsening memory locality and cache efficiency rather than a single threshold or abrupt transition.

4. **The current design explicitly trades memory for read speed.**

   After SSTables are flushed to disk, their contents are also kept resident in memory. This allows GET operations to avoid disk I/O and perform very fast in-memory lookups. However, memory consumption grows with dataset size and lookup latency eventually becomes dominated by memory-access costs rather than binary-search complexity.

5. **The benchmark motivates a block-based SSTable design.**

   The slowdown observed at larger dataset sizes shows why production storage engines do not usually keep full SSTable contents resident in memory. Instead, they keep only compact metadata, indexes, and filters in memory, while loading data blocks on demand through a bounded cache.

## How Production Storage Engines Handle This

TinyKV currently keeps the full contents of flushed SSTables resident in memory. This makes warm reads fast for smaller datasets because GET operations avoid disk I/O entirely. However, the benchmark results show that this design does not scale indefinitely: as the in-memory SSTable representation grows, lookup latency becomes dominated by memory hierarchy effects such as cache misses and poor locality.

Production storage engines generally avoid keeping all SSTable data in memory. Instead, SSTables are commonly divided into smaller blocks:

```txt
[Index Block]
[Filter Block]
[Data Block]
[Data Block]
[Data Block]
...
```

A typical GET flow in a block-based LSM storage engine is:

```txt
Check memtable
↓
Check Bloom filter
↓
Use index block to locate the relevant data block
↓
Read or retrieve only that data block
↓
Search within the block
```

The important difference is that the engine does not need to keep the full SSTable contents in memory. Instead, it keeps a smaller amount of metadata in memory, such as:

- Memtables
- Bloom filters
- Sparse indexes or index blocks
- File metadata and key ranges
- Recently accessed data blocks

Data blocks are loaded on demand. Frequently accessed blocks may be kept in a bounded block cache, often using an eviction policy such as LRU. Once the cache reaches its configured memory budget, older or colder blocks are evicted instead of allowing memory usage to grow without bound.

This means real systems usually do not simply allow RSS to grow until the operating system runs out of memory. They set explicit memory budgets for components such as memtables and block caches. The operating system page cache may still help, but the database also manages its own memory so that performance is more predictable.

Compared to TinyKV's current design:

| Design                        | Memory Usage                                    | Read Behavior                                                               |
| ----------------------------- | ----------------------------------------------- | --------------------------------------------------------------------------- |
| TinyKV current SSTable design | Full SSTable contents are kept in memory        | Fast warm reads, but memory usage and lookup latency grow with dataset size |
| Block-based SSTable design    | Only metadata and hot blocks are kept in memory | Bounded memory usage, better locality, occasional block reads from disk     |

This benchmark therefore motivates a natural next step for TinyKV: replacing fully memory-resident SSTables with block-based SSTables and a bounded block cache. That would let TinyKV retain fast reads for hot data while avoiding linear memory growth with total database size.
