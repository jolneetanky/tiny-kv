
#ifndef SSTABLE_FILE_MANAGER_IMPL_H
#define SSTABLE_FILE_MANAGER_IMPL_H

#include <string>
#include <vector>
#include "../types/entry.h"
#include "../types/error.h"
#include "../types/timestamp.h"
#include "../types/sstable_file.h"
#include "sstable_file_manager.h"
#include "../bloom_filter/bloom_filter.h"

// just a struct for now
class SSTableFileManagerImpl : public SSTableFileManager {
    private:
        std::unique_ptr<SSTableFile> m_ssTableFile;
        BloomFilter m_bloomFilter;

        std::string m_directoryPath;
        std::string m_fname;
        std::string m_fullPath;
        bool m_initialized = false;

        std::string _serializeEntry(const Entry &entry) const;
        std::optional<Entry> _deserializeEntry(const char* data, size_t size, size_t& bytesRead) const;
        bool _writeBinaryToFile(const std::string& path, const std::string& data);
        bool _readBinaryFromFile(const std::string& filename, std::string& outData) const;
        TimestampType _getTimeNow(); // helper to get current timestamp
        std::optional<SSTableFile> _decode(std::string file) const;

        std::optional<Error> _readFileToMemory(); // Reads file, and stores it in-memory within this object.
        std::optional<Error> _init(); // Reads file to memory, and initializes Bloom Filter

    public:
        SSTableFileManagerImpl(std::string directoryPath); // use this to initialize, if the file doesn't exist yet
        SSTableFileManagerImpl(const std::string &directoryPath, const std::string &fileName); // initializes an SSTableFileManager with an existing fileName
        std::optional<Error> write(std::vector<const Entry*> entryPtrs) override;
        std::optional<Entry> get(const std::string& key) override; // searches for a key
        
        std::optional<std::vector<Entry>> getEntries() override;
        std::optional<TimestampType> getTimestamp() override;
        std::string getFullPath() const override;
        std::optional<std::string> getStartKey() override;
        std::optional<std::string> getEndKey() override;

        bool contains(std::string key) override; // might have false positives. But never false negatives.

};

#endif