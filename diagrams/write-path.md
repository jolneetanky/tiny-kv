# TinyKV write path

These sequence diagrams trace `PUT`, `DEL`, and flush from the client through the storage engine. `SkipListImpl` is currently backed by `std::map` despite its name.

## Request and memtable update

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant CLI as CLI and client codec
    participant TCP as FdConnection
    participant Session as Session and server codec
    participant Handler as CommandHandler
    participant DB as DbImpl
    participant Mem as MemTableImpl
    participant Map as SkipListImpl map
    participant Storage as StorageManagerImpl
    participant Level as LevelManagerImpl
    participant WAL as WAL
    participant Flush as MemTableImpl::flushToDisk

    Note over Session: server_main::main() accepts the socket and starts a detached worker that runs Session::run().
    User->>CLI: Enter PUT, DEL, or _FLUSH
    CLI->>CLI: runRepl() validates with protocol::decodeLine()
    CLI->>CLI: protocol::encodeRequest()
    CLI->>TCP: FdConnection::writeAll(request)
    TCP-->>Session: TCP request bytes
    Session->>TCP: FdConnection::readLine()
    TCP-->>Session: Request line
    Session->>Session: protocol::decodeLine()
    Session->>Handler: CommandHandler::execute(command, args)

    alt PUT key value
        Handler->>DB: DbImpl::put(key, value)
        DB->>Mem: MemTableImpl::put(key, value)
        Mem->>Mem: MemTableImpl::isReadOnly()
        Mem->>Map: SkipListImpl::getLength()
        Map-->>Mem: Current map size
        opt Map size equals capacity before this insert
            Note over Mem,Flush: The capacity-filling write remains in memory. The next PUT or DEL flushes before applying its operation.
            Mem->>Flush: MemTableImpl::flushToDisk()
            Flush-->>Mem: Flush subflow completes
        end
        Mem->>WAL: WAL::append(Entry{key, value})
        WAL-->>Mem: Append record and flush stream
        Mem->>Map: SkipListImpl::set(entry), using std::map
        Map-->>Mem: Insert or replace entry
        Mem-->>DB: Success or error
    else DEL key
        Handler->>DB: DbImpl::del(key)
        DB->>Mem: MemTableImpl::del(key)
        Mem->>Mem: MemTableImpl::get(key)
        Mem->>Map: SkipListImpl::get(key)
        Map->>Map: std::map::find(key)
        alt Key absent from memtable
            Mem->>Storage: StorageManagerImpl::get(key)
            loop Each initialized level manager
                Storage->>Level: LevelManagerImpl::getKey(key)
            end
            Level-->>Storage: Entry or nullopt
            Storage-->>Mem: Entry or nullopt
        else Key present in memtable
            Map-->>Mem: Entry
        end
        Mem->>Mem: Reject if key is absent from memtable and storage
        Mem->>Mem: MemTableImpl::isReadOnly()
        Mem->>Map: SkipListImpl::getLength()
        Map-->>Mem: Current map size
        opt Map size equals capacity before tombstone insert
            Mem->>Flush: MemTableImpl::flushToDisk()
            Flush-->>Mem: Flush subflow completes
        end
        Mem->>WAL: WAL::append(tombstone Entry)
        WAL-->>Mem: Append tombstone and flush stream
        Mem->>Map: SkipListImpl::set(tombstone), using std::map
        Map-->>Mem: Insert or replace entry
        Mem-->>DB: Success or error
    else Explicit _FLUSH
        Handler->>DB: DbImpl::forceFlushForTests()
        DB->>Flush: MemTableImpl::flushToDisk()
        Flush-->>DB: Result is currently ignored by forceFlushForTests()
    end

    DB-->>Handler: protocol::Response
    Handler-->>Session: protocol::Response
    Session->>Session: protocol::encodeResponse()
    Session->>TCP: FdConnection::writeAll(response)
    TCP-->>CLI: TCP response bytes
    CLI->>CLI: protocol::decodeResponseLine() and print result
```

## Flush subflow

```mermaid
sequenceDiagram
    autonumber
    participant Mem as MemTableImpl::flushToDisk
    participant Map as SkipListImpl map
    participant Storage as StorageManagerImpl
    participant Level as LevelManagerImpl
    participant Format as Selected TableFormat
    participant Disk as SSTable file
    participant WAL as WAL

    Mem->>Map: SkipListImpl::NewIterator()
    Map-->>Mem: Ordered map iterator
    Mem->>Storage: StorageManagerImpl::write(iterator, 0)
    opt No level managers exist yet
        Storage->>Storage: StorageManagerImpl::initLevels()
        Storage->>Level: LevelManagerImpl::initNew() for existing level directories
        Level-->>Storage: Open existing table readers
    end
    Storage->>Level: LevelManagerImpl::createTable(iterator)
    Level->>Format: TableFormat::writeTable(path, iterator, timestamp, fileNum)
    alt InMemoryTableFormat selected
        Format->>Format: Materialize entries and call SSTableWriter::write()
        Format->>Disk: Write serialized sorted entries and metadata
        Format-->>Level: InMemoryTableReader with entries in memory
    else BlockBasedTableFormat selected
        Format->>Disk: Write data blocks, metadata, index, filter, and footer
        Format-->>Level: BlockBasedTableReader
    end
    Level-->>Storage: Status
    Storage-->>Mem: Write result
    Mem->>WAL: WAL::remove()
    Mem->>Map: SkipListImpl::clear()
    Mem-->>Mem: Mark writable again
```

The current capacity check uses `map.size() == capacity` before inserting the incoming operation. A flush therefore happens on the operation after the map reaches capacity. An explicit flush assumes a non-empty memtable: table formats reject empty tables, and `DbImpl::forceFlushForTests()` currently ignores the error returned by `MemTableImpl::flushToDisk()`. The DB write methods currently do not acquire a mutex.
