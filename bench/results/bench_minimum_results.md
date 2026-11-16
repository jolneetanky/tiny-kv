# bench_minimum benchmark results

Generated on 2026-08-16 from local Release benchmark runs:

```sh
TINYKV_TABLE_FORMAT=in-memory ./build-bench/bench/bench_minimum --benchmark_out=/tmp/tinykv_bench_minimum_in_memory.json --benchmark_out_format=json
TINYKV_TABLE_FORMAT=block-based ./build-bench/bench/bench_minimum --benchmark_out=/tmp/tinykv_bench_minimum_block_based.json --benchmark_out_format=json
```

Notes:

- The benchmark reported an inability to determine CPU clock rate from `sysctl` and a thread-affinity warning. Those affect benchmark metadata, not the measured timings.
- The run was executed on the local benchmark environment reported as `10 X 24 MHz CPU`.
- `TINYKV_TABLE_FORMAT=in-memory` uses the original memory-resident SSTable implementation.
- `TINYKV_TABLE_FORMAT=block-based` uses the block-based SSTable implementation.

## In-memory SSTable Results

| Benchmark | Keys | Value bytes | Time (us) | CPU (us) | Items/s | MiB/s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| BM_PutSequential/1024/64 | 1,024 | 64 | 17,096 | 16,553 | 61.864k | 4.720 |
| BM_PutSequential/4096/64 | 4,096 | 64 | 66,213 | 65,466 | 62.567k | 4.773 |
| BM_PutSequential/4096/256 | 4,096 | 256 | 74,556 | 72,260 | 56.684k | 14.704 |
| BM_PutRandom/1024/64 | 1,024 | 64 | 16,685 | 16,442 | 62.280k | 4.752 |
| BM_PutRandom/4096/64 | 4,096 | 64 | 69,724 | 67,697 | 60.505k | 4.616 |
| BM_PutRandom/4096/256 | 4,096 | 256 | 74,371 | 71,662 | 57.157k | 14.826 |
| BM_GetExistingRandom/1024/64 | 1,024 | 64 | 0.156 | 0.156 | 6.408M | 391.142 |
| BM_GetExistingRandom/4096/64 | 4,096 | 64 | 0.192 | 0.188 | 5.330M | 325.296 |
| BM_GetExistingRandom/4096/256 | 4,096 | 256 | 0.197 | 0.193 | 5.171M | 1262.525 |
| BM_GetMissingRandom/1024/64 | 1,024 | 64 | 0.033 | 0.032 | 31.044M | - |
| BM_GetMissingRandom/4096/64 | 4,096 | 64 | 0.045 | 0.045 | 22.326M | - |
| BM_GetMissingRandom/4096/256 | 4,096 | 256 | 0.045 | 0.045 | 22.280M | - |
| BM_OverwriteRandom/1024/64 | 1,024 | 64 | 17.127 | 16.333 | 61.225k | 4.671 |
| BM_OverwriteRandom/4096/64 | 4,096 | 64 | 17.133 | 16.393 | 61.003k | 4.654 |
| BM_OverwriteRandom/4096/256 | 4,096 | 256 | 18.180 | 17.704 | 56.483k | 14.652 |
| BM_Mixed95Read5Write/1024/64 | 1,024 | 64 | 1.058 | 1.050 | 952.727k | 58.877 |
| BM_Mixed95Read5Write/4096/64 | 4,096 | 64 | 2.317 | 2.298 | 435.243k | 26.897 |
| BM_Mixed95Read5Write/4096/256 | 4,096 | 256 | 2.227 | 2.198 | 454.918k | 111.411 |
| BM_Mixed50Read50Write/1024/64 | 1,024 | 64 | 8.470 | 8.214 | 121.747k | 8.360 |
| BM_Mixed50Read50Write/4096/64 | 4,096 | 64 | 10.297 | 10.121 | 98.809k | 6.785 |
| BM_Mixed50Read50Write/4096/256 | 4,096 | 256 | 10.263 | 10.186 | 98.176k | 24.718 |

## Block-based SSTable Results

| Benchmark | Keys | Value bytes | Time (us) | CPU (us) | Items/s | MiB/s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| BM_PutSequential/1024/64 | 1,024 | 64 | 20,668 | 19,016 | 53.850k | 4.108 |
| BM_PutSequential/4096/64 | 4,096 | 64 | 69,818 | 68,405 | 59.879k | 4.568 |
| BM_PutSequential/4096/256 | 4,096 | 256 | 75,630 | 72,899 | 56.187k | 14.575 |
| BM_PutRandom/1024/64 | 1,024 | 64 | 18,579 | 18,156 | 56.401k | 4.303 |
| BM_PutRandom/4096/64 | 4,096 | 64 | 74,408 | 70,233 | 58.320k | 4.449 |
| BM_PutRandom/4096/256 | 4,096 | 256 | 76,436 | 73,015 | 56.098k | 14.552 |
| BM_GetExistingRandom/1024/64 | 1,024 | 64 | 12.588 | 12.310 | 81.235k | 4.958 |
| BM_GetExistingRandom/4096/64 | 4,096 | 64 | 13.396 | 12.966 | 77.123k | 4.707 |
| BM_GetExistingRandom/4096/256 | 4,096 | 256 | 11.388 | 11.226 | 89.080k | 21.748 |
| BM_GetMissingRandom/1024/64 | 1,024 | 64 | 0.032 | 0.032 | 31.131M | - |
| BM_GetMissingRandom/4096/64 | 4,096 | 64 | 0.044 | 0.044 | 22.824M | - |
| BM_GetMissingRandom/4096/256 | 4,096 | 256 | 0.044 | 0.044 | 22.685M | - |
| BM_OverwriteRandom/1024/64 | 1,024 | 64 | 18.495 | 17.925 | 55.789k | 4.256 |
| BM_OverwriteRandom/4096/64 | 4,096 | 64 | 17.925 | 17.041 | 58.683k | 4.477 |
| BM_OverwriteRandom/4096/256 | 4,096 | 256 | 17.704 | 17.360 | 57.602k | 14.942 |
| BM_Mixed95Read5Write/1024/64 | 1,024 | 64 | 10.667 | 10.573 | 94.585k | 5.845 |
| BM_Mixed95Read5Write/4096/64 | 4,096 | 64 | 20.349 | 20.145 | 49.640k | 3.068 |
| BM_Mixed95Read5Write/4096/256 | 4,096 | 256 | 19.599 | 19.547 | 51.158k | 12.529 |
| BM_Mixed50Read50Write/1024/64 | 1,024 | 64 | 14.947 | 14.911 | 67.063k | 4.605 |
| BM_Mixed50Read50Write/4096/64 | 4,096 | 64 | 52.294 | 51.683 | 19.349k | 1.329 |
| BM_Mixed50Read50Write/4096/256 | 4,096 | 256 | 49.660 | 49.497 | 20.203k | 5.087 |

## Block-based vs In-memory

| Benchmark | In-memory time | Block-based time | Block / In-memory |
| --- | ---: | ---: | ---: |
| BM_PutSequential/1024/64 | 17,096 us | 20,668 us | 1.2x |
| BM_PutSequential/4096/64 | 66,213 us | 69,818 us | 1.1x |
| BM_PutSequential/4096/256 | 74,556 us | 75,630 us | 1.0x |
| BM_PutRandom/1024/64 | 16,685 us | 18,579 us | 1.1x |
| BM_PutRandom/4096/64 | 69,724 us | 74,408 us | 1.1x |
| BM_PutRandom/4096/256 | 74,371 us | 76,436 us | 1.0x |
| BM_GetExistingRandom/1024/64 | 0.156 us | 12.588 us | 80.5x |
| BM_GetExistingRandom/4096/64 | 0.192 us | 13.396 us | 69.9x |
| BM_GetExistingRandom/4096/256 | 0.197 us | 11.388 us | 57.9x |
| BM_GetMissingRandom/1024/64 | 0.033 us | 0.032 us | 1.0x |
| BM_GetMissingRandom/4096/64 | 0.045 us | 0.044 us | 1.0x |
| BM_GetMissingRandom/4096/256 | 0.045 us | 0.044 us | 1.0x |
| BM_OverwriteRandom/1024/64 | 17.127 us | 18.495 us | 1.1x |
| BM_OverwriteRandom/4096/64 | 17.133 us | 17.925 us | 1.0x |
| BM_OverwriteRandom/4096/256 | 18.180 us | 17.704 us | 1.0x |
| BM_Mixed95Read5Write/1024/64 | 1.058 us | 10.667 us | 10.1x |
| BM_Mixed95Read5Write/4096/64 | 2.317 us | 20.349 us | 8.8x |
| BM_Mixed95Read5Write/4096/256 | 2.227 us | 19.599 us | 8.8x |
| BM_Mixed50Read50Write/1024/64 | 8.470 us | 14.947 us | 1.8x |
| BM_Mixed50Read50Write/4096/64 | 10.297 us | 52.294 us | 5.1x |
| BM_Mixed50Read50Write/4096/256 | 10.263 us | 49.660 us | 4.8x |

Write-heavy workloads are broadly similar between the two formats. Sequential and random PUTs are usually within about 0-20% because both formats still write the same logical records through the WAL, memtable, and flush path. The major regression is point-read hits: block-based GET hits are roughly 58-81x slower in this small benchmark because each hit opens the SSTable file and reads the relevant data block, while the in-memory reader searches resident SSTable contents directly. Missing-key reads are effectively unchanged because they are rejected by range/filter checks before data-block reads dominate. Mixed workloads follow the read ratio: 95% reads slows by about 9-10x, while the 50/50 workload slows by about 2-5x.

For the memory tradeoff at larger scale, see [MEMORY_INVESTIGATION_ANALYSIS.md](MEMORY_INVESTIGATION_ANALYSIS.md).
