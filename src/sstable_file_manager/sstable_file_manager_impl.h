
#ifndef SSTABLE_FILE_MANAGER_IMPL_H
#define SSTABLE_FILE_MANAGER_IMPL_H

#include <string>
#include <vector>
#include "../types/entry.h"
#include "../types/error.h"
#include "../types/sstable_file.h"
#include "sstable_file_manager.h"

// just a struct for now
class SSTableFileManagerImpl : public SSTableFileManager {
    private:
        // Stores an in-memory representation of the SSTableFile.
        std::unique_ptr<SSTableFile> m_ssTableFile; // so we can do things in-memory

        std::string m_directoryPath;
        std::string m_fname;
        std::string m_fullPath;
        bool m_initialized = false;

        std::string _serializeEntry(const Entry &entry) const;
        std::optional<Entry> _deserializeEntry(const char* data, size_t size, size_t& bytesRead) const;
        bool _writeBinaryToFile(const std::string& path, const std::string& data);
        bool _readBinaryFromFile(const std::string& filename, std::string& outData) const;
        SSTableFile::TimestampType _getTimeNow(); // helper to get current timestamp
        std::optional<SSTableFile> _decode(std::string file) const;

        // reads the actual file and stores content in `m_ssTableFile`
        // initializes the in-memory `m_ssTableFile`
        std::optional<Error> _readFileToMemory();

    public:
        SSTableFileManagerImpl(std::string directoryPath); // if the file doesn't exist yet
        // initializes an SSTableFileManager with an existing fileName
        SSTableFileManagerImpl(const std::string &directoryPath, const std::string &fileName);
        std::optional<Error> write(std::vector<const Entry*> entryPtrs) override;
        std::optional<Entry> get(const std::string& key) override; // searches for a key
        std::optional<Error> init() override; // allow caller to initialize. If caller doens't initialize, we will just lazy initialize on `get`.
        std::optional<SSTableFile::TimestampType> getTimestamp() override;
};

#endif