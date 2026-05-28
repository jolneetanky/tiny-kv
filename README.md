# TinyKV

TinyKV is a minimal key-value store written in modern C++17.

## Motivation

This is my very first project in C++, meant for me to get a hang of the language and as an introduction to systems programming. I thought a key-value store would be a good place to start due to its feasible implementation.

## Features (Current + Planned)

- [x] Basic in-memory key–value store (CRUD operations)
- [x] Persistent storage with Write-Ahead Log (WAL) and SSTables
- [x] Compaction
- [x] Bloom Filters
- [ ] Benchmarking and Profiling (against LevelDB)
- [ ] Add iterators
- [ ] Add snapshots
- [ ] Concurrency support (thread-safe operations)

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

4. Start the server in one terminal:

```sh
./build/src/tinykv_server
```

5. Start the REPL client in another terminal:

```sh
./build/src/tinykv_cli
```

The server must be running before starting the REPL client.

To use a different port, set `TINYKV_PORT` for both processes:

```sh
TINYKV_PORT=6380 ./build/src/tinykv_server
TINYKV_PORT=6380 ./build/src/tinykv_cli
```

## Example Usage

### CLI usage (primary)

```sh
# Build
cmake -S . -B build
cmake --build build -j

# Terminal 1: start the server
./build/src/tinykv_server

# Terminal 2: run the REPL client
./build/src/tinykv_cli

# PUT a key value pair
PUT <key> <val>

# GET a value
GET <key>

# DELETE a key
DEL <key>

# Exit program
EXIT
```
