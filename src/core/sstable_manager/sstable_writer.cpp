#include "core/sstable_manager/sstable_writer.h"
#include <filesystem>
#include <fstream>
#include <iostream>

TimestampType SSTableWriter::_getTimeNow() const
{
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();

    return timestamp;
};

std::string SSTableWriter::_generateSSTableFileName() const
{
    static std::atomic<uint64_t> counter{0};

    auto now = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                  now.time_since_epoch())
                  .count();

    uint64_t uniqueId = (ns << 16) ^ counter.fetch_add(1); // mix counter + time

    return "table-" + std::to_string(uniqueId);
}

bool SSTableWriter::_createFileIfNotExists(const std::string &fullPath) const
{
    std::filesystem::path fsPath{fullPath};
    std::filesystem::create_directories(fsPath.parent_path());
    std::ofstream outFile(fullPath, std::ios::binary); // open in binary mode
    if (!outFile)
    {
        std::cerr << "Failed to open file for writing: " << fullPath << "\n";
        return false;
    }
    outFile.close();
    return true;
}

// Serializes an `Entry` into the form serialized data: <keyLen><key><valLen><val><tombstone>
std::string SSTableWriter::_serializeEntry(const Entry &entry) const
{
    // std::cout << "[SSTableManagerFileImpl].serializeEntry()" << std::endl;

    std::string out;

    uint32_t keyLen = entry.key.size();
    uint32_t keyLenNetwork = htonl(keyLen); // convert to network byte order (big endian)

    // Append the 4 bytes of key length
    out.append(reinterpret_cast<const char *>(&keyLenNetwork), sizeof(keyLenNetwork));

    // Append the key bytes
    out.append(entry.key);

    // Similarly for value
    uint32_t valLen = entry.val.size();
    uint32_t valLenNetwork = htonl(valLen); // 4 bytes

    // Append the 4 bytes of val length
    out.append(reinterpret_cast<const char *>(&valLenNetwork), sizeof(valLenNetwork)); // 4 bytes
    out.append(entry.val);

    // Append tombstone flag (1 byte)
    char tombstoneFlag = entry.tombstone ? 1 : 0;
    out.append(&tombstoneFlag, 1); // store `tombstoneFlag` as 1 byte. Eg. if tombstoneFlag == 1, the byte would be 00000001

    return out;
};

bool SSTableWriter::_writeBinaryToFile(const std::string &path, const std::string &data)
{
    // create parent directories in path if needed
    std::filesystem::path fsPath{path};
    std::filesystem::create_directories(fsPath.parent_path());
    std::ofstream outFile(path, std::ios::binary); // open in binary mode
    if (!outFile)
    {
        std::cerr << "Failed to open file for writing: " << path << "\n";
        return false;
    }
    outFile.write(data.data(), data.size()); // write raw bytes
    outFile.close();
    return true;
}

SSTableMetadata SSTableWriter::write(const std::string &fname, std::vector<Entry> &entries, TimestampType timestamp, FileNumber file_num)
{

    std::cout << "[SSTableFileManager._writeEntriesToFile()]" << std::endl;

    if (entries.empty())
    {
        throw std::runtime_error("SSTableWriter::write(): Cannot write empty SSTable");
    }

    // sort entries (assume ascending)
    std::sort(entries.begin(), entries.end());

    std::string writeData;

    for (const auto &entry : entries)
    {
        writeData += _serializeEntry(entry);
    }

    writeData.append(reinterpret_cast<const char *>(&timestamp), sizeof(timestamp));
    writeData.append(reinterpret_cast<const char *>(&file_num), sizeof(file_num));

    if (!_writeBinaryToFile(fname, writeData))
    {
        throw std::runtime_error("SSTableWriter::write(): Failed to write SSTable");
    }

    std::cout << "[SSTableFileManager._writeEntriesToFile()] Successfully WRITE SSTable to path " << fname << "\n";

    return SSTableMetadata(file_num, timestamp, entries[0].key, entries[entries.size() - 1].key);
};