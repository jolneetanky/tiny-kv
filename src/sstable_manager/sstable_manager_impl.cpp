#include "sstable_manager_impl.h"
#include <iostream>
#include <cstring>
#include <arpa/inet.h>

#include <fstream>
#include <string>
#include <iostream>

#include <filesystem>

// PRIVATE HELPER FUNCTION
// serialize into a std::string containing binary data
// serialized data: <keyLen><key><valLen><val><tombstone>
std::string SSTableManagerImpl::serializeEntry(const Entry &entry) {
    std::cout << "[SSTableManagerImpl].serializeEntry()" << std::endl;

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
Entry deserializeEntry(const char* data, size_t size, size_t& bytesRead) {
    Entry entry;
    size_t pos = 0;

    // read key length
    if (pos + 4 > size) throw std::runtime_error("Buffer too small for key length");
    uint32_t keyLenNetwork;
    memcpy(&keyLenNetwork, data + pos, 4); // copies 4 characters from the object pointed to by `data + pos`, into `keyLenNetwork`
    pos += 4;
    uint32_t keyLen = ntohl(keyLenNetwork);

    // read key
    if (pos + keyLen > size) throw std::runtime_error("Buffer too small for key");
    entry.key.assign(data + pos, keyLen);
    pos += keyLen;

    // read val length
    if (pos + 4 > size) throw std::runtime_error("Buffer too small for value length");
    uint32_t valLenNetwork;
    memcpy(&valLenNetwork, data + pos, 4);
    pos += 4;
    uint32_t valLen = ntohl(valLenNetwork);

    // read val
    if (pos + valLen > size) throw std::runtime_error("Buffer too small for value");
    entry.val.assign(data + pos, valLen);
    pos += valLen;

    // read tombstone
    if (pos + 1 > size) throw std::runtime_error("Buffer too small for tombstone");
    entry.tombstone = data[pos] != 0 ? true : false;
    pos += 1;

    bytesRead = pos;
    return entry;
}


// Writes the entire content of a binary string to a file
bool writeBinaryToFile(const std::string& path, const std::string& data) {
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

#include <fstream>
#include <string>
#include <iostream>

// Reads entire binary file into a string buffer
bool readBinaryFromFile(const std::string& filename, std::string& outData) {
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


// write index
Error* SSTableManagerImpl::write(std::vector<const Entry*> entries) {
    std::cout << "[SSTableManagerImpl.write()]" << std::endl;
    std::string allSerializedData;
    for (const auto &entryPtr : entries) {
    if (entryPtr == nullptr) {
        std::cerr << "[ERROR] Null entry pointer in entries!\n";
        continue;
    }
       allSerializedData += serializeEntry(*entryPtr);
    }

    std::cout << "[SSTableManagerImpl.write()]" << allSerializedData << std::endl;
    std::string path = "sstables/level-0/table-0";
    if (!writeBinaryToFile(path, allSerializedData)) {
        return &m_writeError;
    }

    read(path);
    return nullptr;
};

// gets entries from a particular SSTable and parses into a vector
std::vector<const Entry*> SSTableManagerImpl::read(std::string file) {
    // read binary from file and store in a string buffer
    std::cout << "[SSTableManagerImpl.read()]" << std::endl;
    std::string serializedEntries;
    readBinaryFromFile(file, serializedEntries);

    //deserialize each entry.
    size_t bytesRead { 0 };
    size_t offset { 0 };
    std::vector<const Entry*> entries;

    while (offset < serializedEntries.size()) {
        Entry entry { deserializeEntry(serializedEntries.data() + offset, serializedEntries.size() - offset, bytesRead)};
        entries.push_back(&entry);
        offset += bytesRead;

        std::cout << "[SSTableManagerImpl.read()]" << " (" << entry.key << ", " << entry.val << ", " << entry.tombstone << ")" << std::endl;
    }

    // // deserialize contents in string buffer into entry
    return entries;
};