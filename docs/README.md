# Docs

Welcome to the TinyKV project documentation.

## Contents

- [Architecture](architecture.md)
- [SSTable File Layout](file_layout.md)
- [Development](development.md)
- [Roadmap](roadmap.md)
- [FAQ](faq.md)

## Quick Start

Build and run from the project root:

```sh
mkdir -p build
cmake -S . -B build
cmake --build build -j
./build/tinykv
```

## Scope

These docs focus on how TinyKV works internally and how to develop on it.
For a project overview and feature list, see `README.md` in the repo root.
