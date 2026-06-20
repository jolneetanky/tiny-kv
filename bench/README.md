# TinyKV Benchmarks

Build benchmarks with:

```sh
cmake -S . -B build-bench -DTINYKV_BUILD_BENCHMARKS=ON -DTINYKV_ENABLE_LOGGING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench -j
```

Run the minimum local suite:

```sh
./build-bench/bench/bench_minimum
```

The minimum suite covers:

- `BM_PutSequential`: sequential load/write path
- `BM_PutRandom`: random load/write path
- `BM_GetExistingRandom`: random point-read hits
- `BM_GetMissingRandom`: random point-read misses
- `BM_OverwriteRandom`: random updates to existing keys
- `BM_Mixed95Read5Write`: read-heavy mixed workload
- `BM_Mixed50Read50Write`: balanced mixed workload

Each benchmark currently runs these dataset shapes:

- 1024 keys, 64 byte values
- 4096 keys, 64 byte values
- 4096 keys, 256 byte values

## Baseline Comparison

Use LevelDB as the first external comparison. It is the closest simple baseline
because TinyKV is also an embedded LSM-style key-value store with a WAL,
memtable, SSTables, and compaction. RocksDB is useful later, but it is much more
tuned and feature-rich, so it is a harsher baseline for this stage.

When comparing, keep these dimensions the same:

- key count
- key size
- value size
- write pattern: sequential or random
- read pattern: random hit or random miss
- filesystem location
- build type, preferably Release

The closest LevelDB `db_bench` mappings are:

- TinyKV `BM_PutSequential` -> LevelDB `fillseq`
- TinyKV `BM_PutRandom` -> LevelDB `fillrandom`
- TinyKV `BM_GetExistingRandom` -> LevelDB `readrandom`
- TinyKV `BM_GetMissingRandom` -> LevelDB `readmissing`
- TinyKV `BM_OverwriteRandom` -> LevelDB `overwrite`
