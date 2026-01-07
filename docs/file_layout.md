# SSTable File Layout

This document describes the on-disk layout produced by the SSTable writer.

## Overview

An SSTable file is a sequence of serialized entries followed by a fixed-size
trailer containing metadata. Entries are written in sorted key order.

```
| Entry 0 | Entry 1 | ... | Entry N | Timestamp | FileNumber |
| 9 bytes | 9 bytes | ... | 9 bytes |
```

This data is serialized into raw bytes, stored in an `std::string`. These bytes

## Entry Encoding

Each entry is encoded as:

```
| key_len (uint32, big-endian) | key bytes |
| val_len (uint32, big-endian) | val bytes |
| tombstone (uint8) |
```

Notes:

- `key_len` and `val_len` are 4-byte unsigned integers in network byte order.
- `tombstone` is a single byte: `1` means deleted, `0` means live.

## File Decoding

The different components of an SSTable file have fixed sizes and are interpreted as such by the SSTable Reader.

As for Entry decoding,

1. `key_len` comes from the first 8 bytes, after which the corresponding number of key bytes are read.
2. `val_len` comes from the next 8 bytes, after which the corresponding number of value bytes are read.
3. Finally, the remaining byte gives us the tombstone value.

## Trailer

After the last entry, the writer appends:

```
| timestamp (TimestampType) | file_num (FileNumber) |
```

Notes:

- `TimestampType` and `FileNumber` are written in host byte order.
- The reader parses these by seeking to the end of the file and reading the
  fixed-size trailer.

## Ordering and Constraints

- Entries are sorted by key before writing.
- Each SSTable contains at least one entry.
- Duplicate keys within a single SSTable are not expected.

## References

- Writer: `src/core/sstable_manager/sstable_writer.cpp`
- Reader: `src/core/sstable_manager/sstable_reader.cpp`
