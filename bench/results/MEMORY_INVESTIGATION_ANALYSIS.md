# Memory Investigation Results

## Summary

This investigation compares TinyKV's original in-memory SSTable representation with the block-based SSTable implementation as dataset size scales. Both runs use 16-byte keys, 64-byte values, a memtable capacity of 1024 entries, and 100,000 warm random reads after loading and flushing each dataset.

Result files:

- In-memory SSTable: [memory_investigation_results_in_memory.csv](memory_investigation_results_in_memory.csv)
- Block-based SSTable: [memory_investigation_results_block_based.csv](memory_investigation_results_block_based.csv)
- The latest generated [memory_investigation_results.csv](memory_investigation_results.csv) currently contains the block-based run.

The block-based run was generated on 2026-08-16 with:

```sh
TINYKV_TABLE_FORMAT=block-based ./build-bench/bench/memory_investigation
```

The in-memory comparison uses the existing matching CSV result set from the same benchmark workload.

## Headline Result

Block-based SSTables dramatically reduce resident memory growth. At 10M keys, peak RSS drops from about 1.39 GB with in-memory SSTables to about 24.8 MB with block-based SSTables, a roughly 56x reduction.

The cost is read-hit latency. For small datasets, in-memory SSTables are much faster because reads are just memory lookups. Block-based reads have a floor around 12-13 us/op because each successful lookup opens the table file and reads/decodes the target data block. At larger datasets, the gap narrows because the in-memory representation starts paying more for cache and locality effects, while the block-based reader keeps memory pressure low.

## Block-based Scaling

|       Keys | log2(Keys) | Peak RSS |  Disk size | SSTable files | Warm read latency |
| ---------: | ---------: | -------: | ---------: | ------------: | ----------------: |
|      1,024 |      10.00 |  1.94 MB |   0.09 MiB |             1 |         12.576 us |
|      4,096 |      12.00 |  2.02 MB |   0.36 MiB |             4 |         12.349 us |
|     16,384 |      14.00 |  2.28 MB |   1.42 MiB |            16 |         12.562 us |
|     65,536 |      16.00 |  2.42 MB |   5.69 MiB |            64 |         12.669 us |
|    250,000 |      17.93 |  2.84 MB |  21.70 MiB |           245 |         13.536 us |
|    500,000 |      18.93 |  3.47 MB |  43.40 MiB |           489 |         15.433 us |
|  1,000,000 |      19.93 |  4.61 MB |  86.80 MiB |           977 |         19.638 us |
|  2,000,000 |      20.93 |  6.58 MB | 173.60 MiB |         1,954 |         27.611 us |
|  4,000,000 |      21.93 | 11.14 MB | 347.20 MiB |         3,907 |         30.549 us |
|  8,000,000 |      22.93 | 20.17 MB | 694.39 MiB |         7,813 |        109.970 us |
| 10,000,000 |      23.25 | 24.81 MB | 867.99 MiB |         9,766 |         48.505 us |

Block-based memory usage grows slowly because the reader keeps metadata, Bloom filters, and index entries in memory, but not full data blocks. Disk usage still scales with total key/value data, as expected.

## In-memory vs Block-based

|       Keys | In-memory peak RSS | Block-based peak RSS | RSS reduction | In-memory latency | Block-based latency | Latency ratio |
| ---------: | -----------------: | -------------------: | ------------: | ----------------: | ------------------: | ------------: |
|      1,024 |            2.59 MB |              1.94 MB |    1.3x lower |          0.268 us |           12.576 us |         46.9x |
|      4,096 |            3.16 MB |              2.02 MB |    1.6x lower |          0.225 us |           12.349 us |         54.9x |
|     16,384 |            5.08 MB |              2.28 MB |    2.2x lower |          0.301 us |           12.562 us |         41.7x |
|     65,536 |           11.94 MB |              2.42 MB |    4.9x lower |          0.457 us |           12.669 us |         27.7x |
|    250,000 |           37.58 MB |              2.84 MB |   13.2x lower |          0.833 us |           13.536 us |         16.3x |
|    500,000 |           72.23 MB |              3.47 MB |   20.8x lower |          1.517 us |           15.433 us |         10.2x |
|  1,000,000 |          141.44 MB |              4.61 MB |   30.7x lower |          2.842 us |           19.638 us |          6.9x |
|  2,000,000 |          279.78 MB |              6.58 MB |   42.5x lower |          6.884 us |           27.611 us |          4.0x |
|  4,000,000 |          556.34 MB |             11.14 MB |   49.9x lower |         22.614 us |           30.549 us |          1.4x |
|  8,000,000 |         1109.56 MB |             20.17 MB |   55.0x lower |         29.770 us |          109.970 us |          3.7x |
| 10,000,000 |         1386.70 MB |             24.81 MB |   55.9x lower |         40.735 us |           48.505 us |          1.2x |

## Interpretation

The in-memory SSTable design optimizes for warm read latency by keeping full SSTable contents resident after flush. This makes point reads extremely fast for small datasets, but RSS grows nearly linearly with total dataset size. By 10M keys, the in-memory run reaches about 1.39 GB RSS.

The block-based SSTable design changes the memory profile. It keeps compact per-table metadata in memory and reads the target data block on demand. That keeps peak RSS under 25 MB even at 10M keys, but successful reads now pay file-open, seek, block-read, and block-decode costs. This explains the roughly 12-13 us/op latency floor at smaller sizes.

At larger sizes, the tradeoff becomes more favorable for block-based SSTables. In-memory reads remain faster for most points, but the difference narrows from roughly 47-55x slower at 1k-4k keys to roughly 1.2x slower at 10M keys, while memory usage is about 56x lower. The 8M block-based point is slower than neighboring points, which likely reflects OS/cache/file-system noise in the run rather than a clean monotonic trend; the overall memory trend is still clear.

## Current Bottleneck

The current block-based reader has no block cache and opens a new `std::ifstream` for each successful `get()`. That makes this benchmark a conservative first result for block-based reads. A production-style next step would be to keep file handles open and add a bounded block cache so hot data blocks can avoid repeated open/read/decode work while preserving bounded memory usage.

## Conclusion

Block-based SSTables achieve the intended memory improvement. They replace linear full-data residency with much smaller metadata residency, reducing 10M-key peak RSS from about 1.39 GB to about 24.8 MB. The current implementation trades away small-dataset read latency because it does on-demand block reads without caching, but the benchmark shows why the design is necessary: it keeps memory usage predictable as the dataset scales.

At 10 million keys, switching from in-memory to block-based SSTables **reduced peak memory usage by about 98.2% (1,386.70 MB → 24.81 MB)**. **Average read-hit latency increased by about 19.1% (40.735 μs → 48.505 μs per read)**.

One caveat: the benchmark times the whole 100,000-read loop and divides by the operation count. **That gives mean latency per operation for the batch**, but it does not capture the distribution or tail latency of individual reads.

## Points of improvement

1. For read latency, we could measure latency of individual reads instead of for the entire batch, and collect percentiles + average + throughput

| Keys | Latency: in-memory (µs) | Latency: block-based (µs) | Block-based latency increase | Peak RSS: in-memory (MiB) | Peak RSS: block-based (MiB) | Block-based RSS reduction |
|---:|---:|---:|---:|---:|---:|---:|
| 1,024 | 0.268 | 12.576 | 4,589% | 2.59 | 1.94 | 25.3% |
| 4,096 | 0.225 | 12.349 | 5,386% | 3.16 | 2.02 | 36.1% |
| 16,384 | 0.301 | 12.562 | 4,069% | 5.08 | 2.28 | 55.1% |
| 65,536 | 0.457 | 12.669 | 2,674% | 11.94 | 2.42 | 79.7% |
| 250,000 | 0.833 | 13.536 | 1,526% | 37.58 | 2.84 | 92.4% |
| 500,000 | 1.517 | 15.433 | 917% | 72.23 | 3.47 | 95.2% |
| 1,000,000 | 2.842 | 19.638 | 591% | 141.44 | 4.61 | 96.7% |
| 2,000,000 | 6.884 | 27.611 | 301% | 279.78 | 6.58 | 97.6% |
| 4,000,000 | 22.614 | 30.549 | 35.1% | 556.34 | 11.14 | 98.0% |
| 8,000,000 | 29.770 | 109.970 | 269% | 1,109.56 | 20.17 | 98.2% |
| 10,000,000 | 40.735 | 48.505 | 19.1% | 1,386.70 | 24.81 | 98.2% |

| # | Issue | Why it matters | Improvement |
|---|---|---|---|
| 1 | **Only one timed batch per dataset, with no separate warm-up.** The benchmark divides one 100,000-GET batch’s time by 100,000, so it reports a mean but no run-to-run variation or tail latency. | A spike like the 8M result could be noise or a stall, and the “warm read” label doesn’t mean a warm-up pass happened. | Run multiple trials, add an untimed warm-up, and report median plus p95/p99 latency. |
| 2 | **The GET sequence is deterministic striding, not random sampling.** It advances by 104,729 modulo the key count. | It visits every key before repeating, but access order may interact with table ordering and caching. For small datasets, it cycles through the same keys many times during the batch. | Use a seeded pseudorandom permutation or seeded random sampling, and report the seed. |
| 3 | **The measurements aren’t isolated from background state.** The benchmark requests compaction after a flush, while the compaction worker can hold the DB mutex during the timed GET loop. Also, dataset sizes run sequentially in one process, and sampled RSS can miss short-lived peaks. | Latency can include compaction lock waits. Later RSS measurements may include memory the allocator retained after earlier runs; RSS sampling every 1,000 GETs can miss brief peaks. | Run each format and dataset size in a fresh process. Either wait for compaction to finish or measure compaction separately. Use an OS peak-RSS metric or sample more frequently. |
