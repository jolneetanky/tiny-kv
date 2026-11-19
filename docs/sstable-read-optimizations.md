# SSTable point-lookup optimization ideas

## Current lookup path

`BlockBasedTableReader::get(key)` first checks the table's key range, then uses
the in-memory index to select the first data block whose `last_key` is at least
`key`. It reads entries from that block in order until it finds an exact match
or reaches a key greater than the target. See
[`BlockBasedTableReader::get()`](../src/core/sstable_manager/block_based/block_based_table_reader.cpp)
and `readEntryFromDataBlock()` in the same file.

This is a linear scan within one block. The writer aims for data blocks of
about 4 KiB, so scanning a block may be a reasonable tradeoff for now. That
limit is not strict: the writer allows at least ten entries per block, and a
large entry can make a block exceed the target size.

## Next optimization points

### 1. Measure point lookups

Before changing the on-disk format, measure lookup latency and record how many
entries and bytes each lookup decodes. Include successful lookups near the
beginning and end of a block, misses inside the table's key range, and misses
outside the range. This will show whether block scanning is a meaningful cost
and whether misses or successful lookups are the bigger problem.

### 2. Use the loaded Bloom filter for negative lookups

The reader already loads the filter block, but `get()` does not currently
check it. Add the membership check after the table-range check and before the
data-block read. A negative filter result can avoid reading a block for an
absent key. A positive result still needs the normal index and block lookup.

### 3. Avoid decoding values while scanning keys

`readEntryFromDataBlock()` currently reads and allocates each value before it
compares that entry's key with the target. The entry encoding stores the value
length after the key, so for an entry whose key is less than the target, the
reader can read the value length and skip the value bytes without allocating a
value string. It only needs to decode the value and tombstone for an exact key
match. This keeps the file format unchanged, though the skip must still check
that it stays within the data block.

### 4. Add a sparse in-block index if scans measure as costly

If profiling shows that scanning within blocks is a bottleneck, store selected
keys and their byte offsets for each data block. For example, with one
checkpoint every 16 entries, lookup can binary-search the checkpoint keys,
seek to the nearest checkpoint at or before the target, then scan forward from
there. With sorted entries, that bounds the scan to roughly one checkpoint
interval in the normal case.

This adds per-block metadata and requires writer and reader format changes.
The offsets must point to entry boundaries, and the checkpoint keys must be
ordered and correspond to those offsets. Because the block encoding uses
variable-length entries, binary-searching raw entry bytes directly is not
reliable without this extra key-to-offset information. Any format change also
needs compatibility handling for existing SSTables.

### 5. Consider a decoded-block cache for repeated reads

If workloads repeatedly access the same blocks, a bounded cache of decoded
blocks can avoid repeated file reads and decoding. It uses additional memory
and needs an eviction policy. This is most useful when measurements show block
reuse; it does not help one-off lookups across many different blocks.

## Suggested order

Measure first, then add the Bloom filter check and avoid decoding values during
the scan. Re-measure after those changes. Consider the sparse in-block index
or a decoded-block cache only if the measured workload still spends meaningful
time reading or scanning data blocks.
