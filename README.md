# TinyKV

TinyKV is a minimal key-value store written in modern C++17.

## Motivation

This is my very first project in C++, meant for me to get a hang of the language and as an introduction to systems programming. I thought a key-value store would be a good place to start due to its feasible implementation.

## Features (Current + Planned)

✅ Basic in-memory key–value store (CRUD operations)
✅ Persistent storage with Write-Ahead Log (WAL) and SSTables
✅ Compaction
✅ Bloom Filters
⏳ Benchmarking and Profiling
⏳ Concurrency support (thread-safe operations)

## Build & Run

1. Clone this repo:

```sh
git clone https://github.com/jolneetanky/tiny-kv.git
```

2. `cd` into it:

```sh
cd tiny-kv
```

3. Build:

```sh
mkdir build
cmake -S . -B build
cmake --build build -j
```

4. Run:

```sh
./build/tinykv
```

## Example Usage

### CLI usage (primary)

```sh
# Build
cmake -S . -B build
cmake --build build -j

# Run the REPL
./build/tinykv

# PUT a key value pair
PUT <key> <val>

# GET a value
GET <key>

# DELETE a key
DEL <key>

# Exit program
EXIT
```
