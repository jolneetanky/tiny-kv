
#ifndef SSTABLE_FILE_MANAGER_IMPL_H
#define SSTABLE_FILE_MANAGER_IMPL_H

#include <string>
#include <vector>
#include "types/entry.h"
#include "types/error.h"
#include "types/timestamp.h"
#include "types/sstable_file.h"
#include "core/sstable_file_manager/sstable_file_manager.h"
#include "core/bloom_filter/bloom_filter.h"
#include <cstdint> // for uint64_t

// contexts
#include "contexts/system_context.h"

// just a struct for now
// invariants: existence of an SSTableFileManager implies the file actually exists.
// it's a logical representation of the existing file.
class SSTableFileManagerImpl : public SSTableFileManager
{
private:
    std::unique_ptr<SSTableFile> m_ssTableFile;
    std::unique_ptr<BloomFilter> m_bloomFilter; // dies tgt with this FileManager

    std::string m_directoryPath;
    std::string m_fname;
    std::string m_fullPath;
    bool m_initialized = false;

    uint64_t m_file_number;

    // contexts
    SystemContext &m_systemContext;

    // helper functions
    std::string _generateSSTableFileName() const;
    bool _createFileIfNotExists(const std::string &fullPath) const;

    std::string _serializeEntry(const Entry &entry) const;
    std::optional<Entry> _deserializeEntry(const char *data, size_t size, size_t &bytesRead) const;
    bool _writeBinaryToFile(const std::string &path, const std::string &data);
    bool _readBinaryFromFile(const std::string &filename, std::string &outData) const;
    TimestampType _getTimeNow(); // helper to get current timestamp
    std::optional<SSTableFile> _decode(std::string file) const;
    std::optional<Error> _writeEntriesToFile(const std::vector<const Entry *> &entryPtrs, const std::string &fname);

    std::optional<Error> _readFileToMemory(); // Reads file, and stores it in-memory within this object.
    std::optional<Error> _init();             // Reads file to memory, and initializes Bloom Filter

public:
    // ctors
    SSTableFileManagerImpl(std::string directoryPath, SystemContext &systemCtx, const std::vector<const Entry *> &entries); // use this to initialize, if the file doesn't exist yet. When this is called, it should create a new file in the directory.
    SSTableFileManagerImpl(const std::string &directoryPath, const std::string &fileName, SystemContext &systemCtx);        // initializes an SSTableFileManager with an existing fileName

    // getters
    std::optional<Entry> get(const std::string &key) override; // searches for a key
    uint64_t getFileNumber() override;
    std::optional<std::vector<Entry>> getEntries() override;
    std::optional<TimestampType> getTimestamp() override;
    std::string getFullPath() const override;
    std::optional<std::string> getStartKey() override;
    std::optional<std::string> getEndKey() override;

    bool contains(std::string key) override; // might have false positives. But never false negatives.
};

#endif