#include "sstable_file_manager_impl.h"
#include <iostream>
#include <filesystem>
#include <chrono> // for std::chrono::system_clock
#include <arpa/inet.h>
#include <fstream>

SSTableFileManagerImpl::SSTableFileManagerImpl(std::string directoryPath) : m_directoryPath{directoryPath} {};
SSTableFileManagerImpl::SSTableFileManagerImpl(const std::string &directoryPath, const std::string &fileName) : m_directoryPath{directoryPath}, m_fname{fileName}, m_fullPath{directoryPath + "/" + fileName} {};

std::string _generateSSTableFileName() {
    static std::atomic<uint64_t> counter{0};

    auto now = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();

    uint64_t uniqueId = (ns << 16) ^ counter.fetch_add(1); // mix counter + time

    return "table-" + std::to_string(uniqueId);
}

// Serializes an `Entry` into the form serialized data: <keyLen><key><valLen><val><tombstone>
std::string SSTableFileManagerImpl::_serializeEntry(const Entry &entry) const {
    // std::cout << "[SSTableManagerFileImpl].serializeEntry()" << std::endl;

    std::string out;

    uint32_t keyLen = entry.key.size();
    uint32_t keyLenNetwork = htonl(keyLen); // convert to network byte order (big endian)

    // Append the 4 bytes of key length
    out.append(reinterpret_cast<const char*>(&keyLenNetwork), sizeof(keyLenNetwork));

    // Append the key bytes
    out.append(entry.key);

    // Similarly for value
    uint32_t valLen = entry.val.size();
    uint32_t valLenNetwork = htonl(valLen); // 4 bytes

    // Append the 4 bytes of val length
    out.append(reinterpret_cast<const char*>(&valLenNetwork), sizeof(valLenNetwork)); // 4 bytes
    out.append(entry.val);

    // Append tombstone flag (1 byte)
    char tombstoneFlag = entry.tombstone ? 1 : 0;
    out.append(&tombstoneFlag, 1); // store `tombstoneFlag` as 1 byte. Eg. if tombstoneFlag == 1, the byte would be 00000001

    return out;
};

// deserialized the serialized binary string
// data: pointer to a `char` array which we are deserializing. Ie `data` is the base address of some char array
// `size`: size of data we are deserializing
std::optional<Entry> SSTableFileManagerImpl::_deserializeEntry(const char* data, size_t size, size_t& bytesRead) const {
    Entry entry;
    size_t pos = 0;

    // read key length
    if (pos + 4 > size) {
        std::string msg { "Buffer too small for key length" };
        std::cout << "[SSTableFileManagerImpl.deserializeEntry()] Failed to deserialize entry: " << msg << "\n";
        return std::nullopt;
    }
    uint32_t keyLenNetwork;
    memcpy(&keyLenNetwork, data + pos, 4); // copies 4 characters from the object pointed to by `data + pos`, into `keyLenNetwork`
    pos += 4;
    uint32_t keyLen = ntohl(keyLenNetwork);

    // read key
    if (pos + keyLen > size) {
        std::string msg { "Buffer too small for key" };
        std::cout << "[SSTableFileManagerImpl.deserializeEntry()] Failed to deserialize entry: " << msg << "\n";
        return std::nullopt;
    };
    entry.key.assign(data + pos, keyLen);
    pos += keyLen;

    // read val length
    if (pos + 4 > size) {
        std::string msg { "Buffer too small for value length" };
        std::cout << "[SSTableFileManagerImpl.deserializeEntry()] Failed to deserialize entry: " << msg << "\n";
        return std::nullopt;
    }

    uint32_t valLenNetwork;
    memcpy(&valLenNetwork, data + pos, 4);
    pos += 4;
    uint32_t valLen = ntohl(valLenNetwork);

    // read val
    if (pos + valLen > size) {
        std::string msg { "Buffer too small for value" };
        std::cout << "[SSTableFileManagerImpl.deserializeEntry()] Failed to deserialize entry: " << msg << "\n";
        return std::nullopt;
    }
    entry.val.assign(data + pos, valLen);
    pos += valLen;

    // read tombstone
    if (pos + 1 > size) {
        std::string msg { "Buffer too small for tombstone" };
        std::cout << "[SSTableFileManagerImpl.deserializeEntry()] Failed to deserialize entry: " << msg << "\n";
        return std::nullopt;
    }

    entry.tombstone = data[pos] != 0 ? true : false;
    pos += 1;

    bytesRead = pos;
    return entry;
}

// Writes the entire content of a binary string to a file
bool SSTableFileManagerImpl::_writeBinaryToFile(const std::string& path, const std::string& data) {
    // create parent directories in path if needed
    std::filesystem::path fsPath{path};
    std::filesystem::create_directories(fsPath.parent_path());
    std::ofstream outFile(path, std::ios::binary);  // open in binary mode
    if (!outFile) {
        std::cerr << "Failed to open file for writing: " << path << "\n";
        return false;
    }
    outFile.write(data.data(), data.size()); // write raw bytes
    outFile.close();
    return true;
}

// Reads entire binary file into a string buffer
bool SSTableFileManagerImpl::_readBinaryFromFile(const std::string& filename, std::string& outData) const {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Failed to open file for reading: " << filename << "\n";
        return false;
    }
    
    // Seek to end to get file size
    inFile.seekg(0, std::ios::end);
    std::streampos fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    
    // Resize string to hold file content
    outData.resize(fileSize);
    
    // Read the whole file into the string buffer
    inFile.read(&outData[0], fileSize);
    
    inFile.close();
    return true;
}

TimestampType SSTableFileManagerImpl::_getTimeNow() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    return timestamp;
}


// gets entries from a particular SSTable and parses into an SSTableFile
std::optional<SSTableFile> SSTableFileManagerImpl::_decode(std::string file) const {
    // read binary from file and store in a string buffer
    std::cout << "[SSTableFileManager.decode()]" << std::endl;
    std::string serializedData; //stores the binary. `readBinaryFromFile` will modify this item

    if (!_readBinaryFromFile(file, serializedData)) {
        std::cerr << "[SSTableFileManager.decode()] Failed to read binary from file" << std::endl;
        return std::nullopt;
    }

    // extract timestamp from the last 8 bytes
    TimestampType timestamp;
    char* timestampSrc = serializedData.data() + serializedData.size() - sizeof(TimestampType);

    std::memcpy(&timestamp, timestampSrc, sizeof(TimestampType));

    //deserialize each entry.
    size_t bytesRead { 0 };
    size_t offset { 0 };
    size_t serializedEntriesSize { serializedData.size() - sizeof(TimestampType) };
    std::vector<Entry> entries;

    while (offset < serializedEntriesSize) {
        const char *data { serializedData.data() };
        std::optional<Entry> optEntry { _deserializeEntry(data + offset, serializedEntriesSize - offset, bytesRead)};

        if (!optEntry) {
            return std::nullopt;
        }

        entries.push_back(optEntry.value());
        offset += bytesRead;

        std::cout << "[SSTableFileManager.read()]" << " (" << optEntry->key << ", " << optEntry->val << ", " << optEntry->tombstone << ")" << std::endl;
    }

    std::cout << "[SSTableFileManager.read()] Successfully read" "\n";
    return SSTableFile{ entries, timestamp };
};


std::optional<Error> SSTableFileManagerImpl::_readFileToMemory() {
    std::cout << "SSTableFileManagerImpl.readFileToMemory()" << "\n";

    if (!m_ssTableFile) {
        std::optional<SSTableFile> ssTableFileOpt { _decode(m_fullPath) };
        if (!ssTableFileOpt) {
            std::cerr << "SSTableFileManagerImpl.readFileToMemory() Failed to read file to memory" << "\n";
            return Error{"Failed to read file to memory"};
        }

        m_ssTableFile = std::make_unique<SSTableFile>(ssTableFileOpt->entries, ssTableFileOpt->timestamp);
    }
    return std::nullopt;
};

// NOTE: this impl will overwrite existing `m_fname` and `m_fullPath` if those have alr been initialized.
std::optional<Error> SSTableFileManagerImpl::write(std::vector<const Entry*> entryPtrs) {

    std::cout << "[SSTableFileManager.write()]" << std::endl;

    TimestampType timestamp { _getTimeNow() };
    std::string fname { _generateSSTableFileName() };
    std::string fullPath { m_directoryPath + "/" + fname };
    m_fullPath = fullPath;
    m_fname = fname;

    std::vector<Entry> entries;

    std::string writeData;

    for (const auto &entryPtr : entryPtrs) {
        if (entryPtr == nullptr) {
            std::cerr << "[SSTableFileManager.write()] Failed to write: Null entry pointer in entries!\n";
            return Error{ "Failed to write to SSTable"};
        }

        entries.push_back(*entryPtr); // copied into vector
        writeData += _serializeEntry(*entryPtr);
    }


    writeData.append(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));

    std::cout << "[SSTableFileManager.write()]" << writeData << std::endl;
    if (!_writeBinaryToFile(fullPath, writeData)) {
        std::cerr << "[SSTableFileManager.write()] Failed to write to SSTable";
        return Error{ "Failed to write to SSTable"};
    }

    m_ssTableFile = std::make_unique<SSTableFile>(entries, timestamp); // values all copied in, not reference
    m_initialized = true;

    std::cout << "[SSTableFileManager.write()] Successfully WRITE SSTable to path " << fullPath << "\n";
    return std::nullopt;
};

// returns the first entry found - tombstone or not.
// else sometimes the (most recent) entry has been found and it's tombstoned,
// but because we tell the caller not found, they continue searching in the other files.
std::optional<Entry> SSTableFileManagerImpl::get(const std::string& key) {
    std::cout << "[SSTableFileManagerImpl.get()]" << "\n";

    // initialize if needed
    if (!m_initialized) {
        std::optional<Error> err { _init() };
        if (err) {
            std::cerr << "[SSTableFileManagerImpl.get()] Failed to read file to memory: " << err->error << "\n";
            return std::nullopt;
        }
    }

    const std::vector<Entry> &entries = m_ssTableFile->entries;

    if (entries.size() == 0) {
        return std::nullopt;
    }

    for (auto &entry : entries) {
        std::cout << "[SSTableFileManagerImpl.get()] (" << entry.key << ", " << entry.val << ", " << entry.tombstone << ")" << "\n";
    }

    // binary search :)
    int l = 0;
    int r = entries.size() - 1;

    // assume within an SSTable there are no duplicate keys.
    while (l < r) {
        int mid = l + ((r - l) / 2);
        const Entry& midEntry = entries[mid];

        if (midEntry.key == key) {
            std::cout << "[SSTableFileManagerImpl.get()] FOUND: " << "\n";
            return midEntry;
        }

        if (key < midEntry.key) {
            r = mid - 1;
        } else {
            l = mid + 1;
        }
    }

    if (entries[l].key == key) {
        std::cout << "[SSTableFileManagerImpl.get()] FOUND" << "\n";
        return entries[l];
    }

    std::cout << "[SSTableFileManagerImpl.get()] NOT FOUND" << "\n";
    return std::nullopt;
};

std::optional<TimestampType> SSTableFileManagerImpl::getTimestamp() {
    if (!m_initialized) {
        _init();
    }

    return m_ssTableFile->timestamp;
};

std::optional<std::vector<Entry>> SSTableFileManagerImpl::getEntries() {
    if (!m_initialized) {
        _init();
    }

    return m_ssTableFile->entries;
};

// reads entries to mmemory. In future this should be hidden.
std::optional<Error> SSTableFileManagerImpl::_init() {
    std::cout << "[SSTableFileManagerImpl.init()]" << "\n";
    
    if (const auto &err = _readFileToMemory()) {
        std::cout << "[SSTableFileManagerImpl.init()] Failed to initialize file" << "\n";
        return err;
    }

    m_initialized = true;

    return std::nullopt;
};


std::string SSTableFileManagerImpl::getFullPath() const {
    return m_fullPath;
};

std::optional<std::string> SSTableFileManagerImpl::getStartKey() {
    if (!m_initialized) {
        _init(); 
        // std::cout << "[SSTableFileManager.getStartKey()] Failed to get start key: file manager not yet initialized" << "\n";
        // return std::nullopt;
    }

    if (m_ssTableFile->entries.size() == 0) {
        return std::nullopt;
    }

    return m_ssTableFile->entries[0].key;
};

std::optional<std::string> SSTableFileManagerImpl::getEndKey() {
    if (!m_initialized) {
        std::cout << "[SSTableFileManager.getEndKey()] Failed to get start key: file manager not yet initialized" << "\n";
        return std::nullopt;
    }

    if (m_ssTableFile->entries.size() == 0) {
        return std::nullopt;
    }

    return m_ssTableFile->entries.back().key;
};


bool SSTableFileManagerImpl::contains(std::string key) {
    // check bloom filter if the entry exists
}; 