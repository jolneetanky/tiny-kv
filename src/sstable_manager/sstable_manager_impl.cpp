#include "sstable_manager_impl.h"
#include "../sstable_file_manager/sstable_file_manager_impl.h"
#include <string>
#include <iostream>

// #include <filesystem>
// #include <chrono> // for std::chrono::system_clock

// Serializes an `Entry` into the form serialized data: <keyLen><key><valLen><val><tombstone>
// std::string SSTableManagerImpl::serializeEntry(const Entry &entry) {
//     std::cout << "[SSTableManagerImpl].serializeEntry()" << std::endl;

//     std::string out;

//     uint32_t keyLen = entry.key.size();
//     uint32_t keyLenNetwork = htonl(keyLen); // convert to network byte order (big endian)

//     // Append the 4 bytes of key length
//     out.append(reinterpret_cast<const char*>(&keyLenNetwork), sizeof(keyLenNetwork));

//     // Append the key bytes
//     out.append(entry.key);

//     // Similarly for value
//     uint32_t valLen = entry.val.size();
//     uint32_t valLenNetwork = htonl(valLen); // 4 bytes

//     // Append the 4 bytes of val length
//     out.append(reinterpret_cast<const char*>(&valLenNetwork), sizeof(valLenNetwork)); // 4 bytes
//     out.append(entry.val);

//     // Append tombstone flag (1 byte)
//     char tombstoneFlag = entry.tombstone ? 1 : 0;
//     out.append(&tombstoneFlag, 1); // store `tombstoneFlag` as 1 byte. Eg. if tombstoneFlag == 1, the byte would be 00000001

//     return out;
// };

// // deserialized the serialized binary string
// // data: pointer to a `char` array which we are deserializing. Ie `data` is the base address of some char array
// // `size`: size of data we are deserializing
// std::optional<Entry> SSTableManagerImpl::deserializeEntry(const char* data, size_t size, size_t& bytesRead) {
//     Entry entry;
//     size_t pos = 0;

//     // read key length
//     if (pos + 4 > size) {
//         std::string msg { "Buffer too small for key length" };
//         std::cout << "[SSTableManagerImpl.deserializeEntry()] Failed to deserialize entry: " << msg << "\n";
//         return std::nullopt;
//     }
//     uint32_t keyLenNetwork;
//     memcpy(&keyLenNetwork, data + pos, 4); // copies 4 characters from the object pointed to by `data + pos`, into `keyLenNetwork`
//     pos += 4;
//     uint32_t keyLen = ntohl(keyLenNetwork);

//     // read key
//     if (pos + keyLen > size) {
//         std::string msg { "Buffer too small for key" };
//         std::cout << "[SSTableManagerImpl.deserializeEntry()] Failed to deserialize entry: " << msg << "\n";
//         return std::nullopt;
//     };
//     entry.key.assign(data + pos, keyLen);
//     pos += keyLen;

//     // read val length
//     if (pos + 4 > size) {
//         std::string msg { "Buffer too small for value length" };
//         std::cout << "[SSTableManagerImpl.deserializeEntry()] Failed to deserialize entry: " << msg << "\n";
//         return std::nullopt;
//     }

//     uint32_t valLenNetwork;
//     memcpy(&valLenNetwork, data + pos, 4);
//     pos += 4;
//     uint32_t valLen = ntohl(valLenNetwork);

//     // read val
//     if (pos + valLen > size) {
//         std::string msg { "Buffer too small for value" };
//         std::cout << "[SSTableManagerImpl.deserializeEntry()] Failed to deserialize entry: " << msg << "\n";
//         return std::nullopt;
//     }
//     entry.val.assign(data + pos, valLen);
//     pos += valLen;

//     // read tombstone
//     if (pos + 1 > size) {
//         std::string msg { "Buffer too small for tombstone" };
//         std::cout << "[SSTableManagerImpl.deserializeEntry()] Failed to deserialize entry: " << msg << "\n";
//         return std::nullopt;
//     }

//     entry.tombstone = data[pos] != 0 ? true : false;
//     pos += 1;

//     bytesRead = pos;
//     return entry;
// }


// // Writes the entire content of a binary string to a file
// bool SSTableManagerImpl::writeBinaryToFile(const std::string& path, const std::string& data) {
//     // create parent directories in path if needed
//     std::filesystem::path fsPath{path};
//     std::filesystem::create_directories(fsPath.parent_path());
//     std::ofstream outFile(path, std::ios::binary);  // open in binary mode
//     if (!outFile) {
//         std::cerr << "Failed to open file for writing: " << path << "\n";
//         return false;
//     }
//     outFile.write(data.data(), data.size()); // write raw bytes
//     outFile.close();
//     return true;
// }

// // Reads entire binary file into a string buffer
// bool SSTableManagerImpl::readBinaryFromFile(const std::string& filename, std::string& outData) {
//     std::ifstream inFile(filename, std::ios::binary);
//     if (!inFile) {
//         std::cerr << "Failed to open file for reading: " << filename << "\n";
//         return false;
//     }
    
//     // Seek to end to get file size
//     inFile.seekg(0, std::ios::end);
//     std::streampos fileSize = inFile.tellg();
//     inFile.seekg(0, std::ios::beg);
    
//     // Resize string to hold file content
//     outData.resize(fileSize);
    
//     // Read the whole file into the string buffer
//     inFile.read(&outData[0], fileSize);
    
//     inFile.close();
//     return true;
// }

// SSTableFile::TimestampType SSTableManagerImpl::getTimestamp() {
//     auto now = std::chrono::system_clock::now();
//     auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
//         now.time_since_epoch()
//     ).count();

//     return timestamp;
// }


// // gets entries from a particular SSTable and parses into an SSTableFile
// std::optional<SSTableFile> SSTableManagerImpl::read(std::string file) {
//     // read binary from file and store in a string buffer
//     std::cout << "[SSTableManagerImpl.read()]" << std::endl;
//     std::string serializedData;

//     if (!readBinaryFromFile(file, serializedData)) {
//         std::cerr << "[SSTableManagerImpl.read()] Failed to read binary from file" << std::endl;
//         return std::nullopt;
//     }

//     // extract timestamp from the last 8 bytes
//     SSTableFile::TimestampType timestamp;
//     char* timestampSrc = serializedData.data() + serializedData.size() - sizeof(SSTableFile::TimestampType);

//     std::memcpy(&timestamp, timestampSrc, sizeof(SSTableFile::TimestampType));

//     //deserialize each entry.
//     size_t bytesRead { 0 };
//     size_t offset { 0 };
//     size_t serializedEntriesSize { serializedData.size() - sizeof(SSTableFile::TimestampType) };
//     std::vector<Entry> entries;

//     while (offset < serializedEntriesSize) {
//         const char *data { serializedData.data() };
//         // std::optional<Entry> optEntry { deserializeEntry(serializedEntries.data() + offset, serializedEntries.size() - offset, bytesRead)};
//         std::optional<Entry> optEntry { deserializeEntry(data + offset, serializedEntriesSize - offset, bytesRead)};

//         if (!optEntry) {
//             return std::nullopt;
//         }

//         entries.push_back(optEntry.value());
//         offset += bytesRead;

//         std::cout << "[SSTableManagerImpl.read()]" << " (" << optEntry->key << ", " << optEntry->val << ", " << optEntry->tombstone << ")" << std::endl;
//     }

//     return SSTableFile{ entries, timestamp };
// };

std::vector<SSTableFileManager*> SSTableManagerImpl::getFilesFromDirectory(const std::string &dirName) {
    std::vector<SSTableFileManager*> files;
    for (auto& uptr : m_ssTableFileManagers) {
        files.push_back(uptr.get());
    }
    return files;
}

// write all entries into a file (serialize each entry)
std::optional<Error> SSTableManagerImpl::write(std::vector<const Entry*> entries) {
    // create a new SSTableFileManager with directory path 0
    // make a copy and append to m_ssTableManagers

    // create the new file manager and append to our list of file managers
    // SSTableFileManagerImpl newFileManager { fullPath };

    std::cout << "[SSTableManagerImpl.write()]" << std::endl;

    // SSTableFile::TimestampType timestamp { getTimestamp() };
    // std::string fname { "table-" + std::to_string(timestamp) };
    // std::string fullPath { LEVEL_0_DIRECTORY + fname };

    m_ssTableFileManagers.push_back(std::make_unique<SSTableFileManagerImpl>(LEVEL_0_DIRECTORY));

    // Get a reference to the newly added manager
    SSTableFileManager* newManager = m_ssTableFileManagers.back().get();
    
    std::optional<Error> errOpt { newManager->write(entries) };
    if (errOpt) {
        std::cerr << "[SSTableManagerImpl.write()] Failed to write to SSTable";
        return errOpt;
    }

    return std::nullopt;

    // std::string writeData;
    // for (const auto &entryPtr : entries) {
    // if (entryPtr == nullptr) {
    //     std::cerr << "[SSTableManagerImpl.write()] Failed to write: Null entry pointer in entries!\n";
    //     return Error{ "Failed to write to SSTable"};
    // }
    //    writeData += serializeEntry(*entryPtr);
    // }

    // // SSTableFile::TimestampType timestamp { getTimestamp() };
    // writeData.append(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));

    // std::cout << "[SSTableManagerImpl.write()]" << writeData << std::endl;
    // if (!writeBinaryToFile(fullPath, writeData)) {
    //     std::cerr << "[SSTableManagerImpl.write()] Failed to write to SSTable";
    //     return Error{ "Failed to write to SSTable"};
    // }


    // std::cout << "[SSTableManagerImpl.write()] Successfully WRITE SSTable to path " << fullPath << "\n";
    // return std::nullopt;
};

std::optional<Entry> SSTableManagerImpl::get(const std::string& key) {
    std::cout << "[SSTableManagerImpl.get()]" << "\n";
    // TODO: iterate through the whole of level 0,
    // for now get all the files in level 0

    // LOCK DIRECTORY ()
    // NOTE: this is a copy of pointers, modifying it in-place wont change the order of ptrs in ssTablefileManagers
    std::vector<SSTableFileManager*> ssTableFileManagers { getFilesFromDirectory(LEVEL_0_DIRECTORY)}; // makes a copy

    std::sort(ssTableFileManagers.begin(), ssTableFileManagers.end(),
        [](const SSTableFileManager* a, const SSTableFileManager* b) {
            auto ta = a->getTimestamp();
            auto tb = b->getTimestamp();

            // If both have timestamp, compare their values
            if (ta && tb) {
                return ta.value() < tb.value();
            }

            // If only a has timestamp, it should come after b (so b first)
            if (ta && !tb) return false;

            // If only b has timestamp, it should come after a
            if (!ta && tb) return true;

            // If neither has timestamp, consider equal
            return false;
        }
    );

    for (const auto& fileManager : ssTableFileManagers) {
        std::cout << "[SSTableManagerImpl.find()] HIII" << "\n";
        std::cout << "[SSTableManagerImpl.find()] " << std::to_string(fileManager->getTimestamp().value()) << "\n";

        // now search in order
        fileManager->get(key);
    }

    return std::nullopt;
}