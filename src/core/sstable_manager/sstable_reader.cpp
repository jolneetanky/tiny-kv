#include "core/sstable_manager/sstable_reader.h"
#include <iostream>
#include <fstream>
#include "common/log.h"

SSTable SSTableReader::read(const std::string &full_path)
{
    // read binary from file and store in a string buffer
    TINYKV_LOG("[SSTableReader.decode()]");
    std::string serializedData; // stores the binary. `readBinaryFromFile` will modify this item

    if (!_readBinaryFromFile(full_path, serializedData))
    {
        std::cerr << "[SSTableReader.decode()] Failed to read binary from file" << std::endl;
        throw std::runtime_error("SSTableReader::read(): Failed to read binary from file");
    }

    // EDGE CASE: File is empty for some reason
    // If file is empty, return nullopt
    if (serializedData.size() == 0)
    {
        throw std::runtime_error("SSTableReader::read(): Failed to read binary from file - file is empty");
    }

    size_t file_num_size = sizeof(FileNumber);
    size_t timestamp_size = sizeof(TimestampType);

    // extract timestamp
    TimestampType timestamp;
    char *timestampSrc = serializedData.data() + (serializedData.size() - file_num_size - timestamp_size);

    std::memcpy(&timestamp, timestampSrc, timestamp_size); // copy the bytes exactly as they are, into `timestamp`

    // extract file number
    FileNumber file_num;
    char *file_num_src = serializedData.data() + (serializedData.size() - file_num_size);

    std::memcpy(&file_num, file_num_src, file_num_size);

    // deserialize each entry.
    size_t bytesRead{0};
    size_t offset{0};
    size_t serializedEntriesSize{serializedData.size() - sizeof(TimestampType)};
    std::vector<Entry> entries;

    while (offset < serializedEntriesSize)
    {
        const char *data{serializedData.data()};
        Entry entry{_deserializeEntry(data + offset, serializedEntriesSize - offset, bytesRead)};

        offset += bytesRead;

        TINYKV_LOG("[SSTableReader.decode()]" << " (" << entry.key << ", " << entry.val << ", " << entry.tombstone << ")");
    }

    SSTableMetadata metadata(file_num, timestamp, entries[0].key, entries[entries.size() - 1].key);
    return SSTable(metadata, std::move(entries));
};

Entry SSTableReader::_deserializeEntry(const char *data, size_t size, size_t &bytesRead) const
{
    Entry entry;
    size_t pos = 0;

    // read key length
    if (pos + 4 > size)
    {
        std::string msg{"Buffer too small for key length"};
        throw std::runtime_error("[SSTableReader._deserializeEntry()] Failed to deserialize entry: " + msg);
    }
    uint32_t keyLenNetwork;
    memcpy(&keyLenNetwork, data + pos, 4); // copies 4 characters from the object pointed to by `data + pos`, into `keyLenNetwork`
    pos += 4;
    uint32_t keyLen = ntohl(keyLenNetwork);

    // read key
    if (pos + keyLen > size)
    {
        std::string msg{"Buffer too small for key"};
        throw std::runtime_error("[SSTableReader._deserializeEntry()] Failed to deserialize entry: " + msg);
    };
    entry.key.assign(data + pos, keyLen);
    pos += keyLen;

    // read val length
    if (pos + 4 > size)
    {
        std::string msg{"Buffer too small for value length"};
        throw std::runtime_error("[SSTableReader._deserializeEntry()] Failed to deserialize entry: " + msg);
    }

    uint32_t valLenNetwork;
    memcpy(&valLenNetwork, data + pos, 4);
    pos += 4;
    uint32_t valLen = ntohl(valLenNetwork);

    // read val
    if (pos + valLen > size)
    {
        std::string msg{"Buffer too small for value"};
        throw std::runtime_error("[SSTableReader._deserializeEntry()] Failed to deserialize entry: " + msg);
    }
    entry.val.assign(data + pos, valLen);
    pos += valLen;

    // read tombstone
    if (pos + 1 > size)
    {
        std::string msg{"Buffer too small for tombstone"};
        throw std::runtime_error("[SSTableReader._deserializeEntry()] Failed to deserialize entry: " + msg);
    }

    entry.tombstone = data[pos] != 0 ? true : false;
    pos += 1;

    bytesRead = pos;
    return entry;
}

bool SSTableReader::_readBinaryFromFile(const std::string &filename, std::string &outData) const
{
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile)
    {
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