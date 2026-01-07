# Development

## Build

```sh
mkdir -p build
cmake -S . -B build
cmake --build build -j
```

## Run

```sh
./build/tinykv
```

## Tests

If tests exist in `tests/`, add the preferred runner here.

```sh
# Example placeholder
# ctest --test-dir build
```

## Project Structure

- `src/`: core implementation
- `tests/`: unit and integration tests
- `benchmarks/`: benchmarks and performance checks
- `wal/`: WAL storage (runtime data)
- `build/`: build artifacts

## Debugging Tips

- Use `-DCMAKE_BUILD_TYPE=Debug` for debug symbols.
- Consider adding lightweight logging at component boundaries.
