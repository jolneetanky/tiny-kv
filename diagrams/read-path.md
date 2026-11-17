# TinyKV read path

The first diagram follows a `GET` from the CLI to the database and back. The second expands the storage lookup beneath `StorageManagerImpl::get()`, including what each table reader does internally. `SkipListImpl` is backed by `std::map` despite its name.

## Request and response path

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant CLI as runRepl()
    participant TCP as FdConnection
    participant Session as Session
    participant Handler as CommandHandler
    participant DB as DbImpl
    participant Mem as MemTableImpl
    participant Map as SkipListImpl and std::map
    participant Storage as StorageManagerImpl

    Note over Session: server_main accepts the socket and starts a worker that calls Session::run().
    User->>CLI: Type GET key
    CLI->>CLI: protocol::decodeLine(line, request, error)
    CLI->>CLI: protocol::encodeRequest(request)
    CLI->>TCP: FdConnection::writeAll(encodedRequest)
    TCP-->>Session: Request bytes over TCP
    Session->>TCP: FdConnection::readLine()
    TCP-->>Session: Request line
    Session->>Session: protocol::decodeLine(line, request, error)
    Session->>Handler: CommandHandler::execute(GET, args)
    Handler->>DB: DbImpl::get(key)
    DB->>Mem: MemTableImpl::get(key)
    Mem->>Map: SkipListImpl::get(key)
    Map->>Map: std::map::find(key)

    alt Memtable contains a value
        Map-->>Mem: Entry with value
        Mem-->>DB: Entry with value
        DB->>DB: Build successful response with Entry value
    else Memtable contains a tombstone
        Map-->>Mem: Entry marked tombstone
        Mem-->>DB: Entry marked tombstone
        DB->>DB: Build key-not-found response without disk lookup
    else Key is absent from memtable
        Map-->>Mem: nullopt
        Mem-->>DB: nullopt
        DB->>Storage: StorageManagerImpl::get(key)
        Note over Storage: Detailed per-level and per-table lookup is shown below.
        Storage-->>DB: Entry or nullopt
        alt Storage returned a value entry
            DB->>DB: Build successful response with Entry value
        else Storage returned nullopt
            DB->>DB: Build key-not-found response
        end
    end

    DB-->>Handler: protocol::Response
    Handler-->>Session: protocol::Response
    Session->>Session: protocol::encodeResponse(response)
    Session->>TCP: FdConnection::writeAll(encodedResponse)
    TCP-->>CLI: Response bytes over TCP
    CLI->>TCP: FdConnection::readLine()
    TCP-->>CLI: Response line
    CLI->>CLI: protocol::decodeResponseLine(line, response, error)
    CLI->>User: Print value, OK, or error
```

## Storage and table-reader lookup

```mermaid
sequenceDiagram
    autonumber
    participant DB as DbImpl::get
    participant Storage as StorageManagerImpl::get
    participant Level as LevelManagerImpl::getKey
    participant Reader as TableReader
    participant InMem as InMemoryTableReader
    participant SST as SSTable
    participant Block as BlockBasedTableReader
    participant File as SSTable file
    participant Decode as Block read helpers

    DB->>Storage: get(key) after memtable miss
    loop Each level manager in m_levelManagers, in vector order
        Storage->>Level: LevelManagerImpl::getKey(key)
        loop Each tableReader in this level, in vector order
            Level->>Reader: TableReader::withinRange(key)
            alt Key outside table's inclusive range
                Reader-->>Level: false
                Level->>Level: Continue to next tableReader
            else Key may be in this table
                Reader-->>Level: true
                alt InMemoryTableReader selected
                    Level->>InMem: InMemoryTableReader::get(key)
                    InMem->>SST: SSTable::get(key)
                    SST->>SST: Binary search sorted m_entries
                    alt Matching key found
                        SST-->>InMem: Entry
                    else No matching key
                        SST-->>InMem: nullopt
                    end
                    InMem-->>Level: Entry or nullopt
                else BlockBasedTableReader selected
                    Level->>Block: BlockBasedTableReader::get(key)
                    Block->>Block: withinRange(key) checks m_meta min and max keys
                    alt Key outside metadata range
                        Block-->>Level: nullopt
                    else Key inside metadata range
                        Block->>Block: std::lower_bound compares IndexEntry::last_key with key
                        alt No matching index entry
                            Block-->>Level: nullopt
                        else Index entry points to candidate data block
                            Note over Block: The chosen IndexEntry contains last_key and a BlockHandle with offset and size.
                            Block->>File: Open m_fullPath with std::ifstream
                            Block->>Decode: readEntryFromDataBlock(input, block handle, key, path)
                            Decode->>File: seekg(block offset)
                            Decode->>Decode: readUint32BigEndian(input) reads the entry count
                            Decode->>File: input.read() reads the four count bytes
                            loop Entries in selected block, in sorted key order
                                Decode->>Decode: readString(input) for entry key
                                Decode->>Decode: readUint32BigEndian(input) reads key length
                                Decode->>File: input.read() reads key bytes
                                Decode->>Decode: readString(input) for entry value
                                Decode->>Decode: readUint32BigEndian(input) reads value length
                                Decode->>File: input.read() reads value bytes
                                Decode->>File: input.read() reads tombstone byte
                                alt Entry key equals requested key
                                    Decode->>Decode: Construct Entry{key, value, tombstone}
                                    Decode-->>Block: Return Entry
                                else Entry key sorts after requested key
                                    Decode-->>Block: Stop early and return nullopt
                                else Entry key sorts before requested key
                                    Decode->>Decode: Continue to next encoded entry
                                end
                            end
                            Block-->>Level: Entry or nullopt
                        end
                    end
                end

                alt Reader returned an Entry
                    alt Entry is a tombstone
                        Level->>Level: Stop scanning this level at this table
                        Note over Level,Storage: getKey() then returns nullopt for the tombstone
                    else Entry is a value
                        Level-->>Storage: Return Entry immediately
                    end
                else Reader returned nullopt
                    Level->>Level: Continue to next tableReader
                end
            end
        end
        Level-->>Storage: Entry or nullopt after table scan
        alt Entry is a value
            Storage-->>DB: Return Entry immediately
        else nullopt
            Storage->>Storage: Continue to next level manager
        end
    end
    Storage-->>DB: nullopt after all levels miss
```

For block-based tables, `BlockBasedTableReader` loads the footer, filter, index, and metadata when its reader is opened. A point lookup uses the metadata range and index; the current `get()` does not consult the loaded filter. It opens the table file and reads only the candidate data block. For in-memory tables, `InMemoryTableReader::get()` delegates to `SSTable::get()`, which binary-searches entries materialized when the reader was opened.

`StorageManagerImpl::initLevels()` is called lazily by `StorageManagerImpl::write()`, not by `get()` or server startup. Before a write initializes the level managers, a memtable miss may not search existing SSTables on disk. There is also a tombstone limitation: `LevelManagerImpl::getKey()` stops at a tombstone but returns `nullopt`, so `StorageManagerImpl::get()` cannot distinguish it from a miss and may continue into a deeper level. A tombstone in the memtable does stop the lookup in `DbImpl::get()`.
