
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
        std::unique_ptr<SSTableFile> m_ssTableFile;

        std::string m_directoryPath;
        std::string m_fname;
        std::string m_fullPath;
        std::mutex m_mutex;

        std::string serializeEntry(const Entry &entry) const;
        std::optional<Entry> deserializeEntry(const char* data, size_t size, size_t& bytesRead) const;
        bool writeBinaryToFile(const std::string& path, const std::string& data);
        bool readBinaryFromFile(const std::string& filename, std::string& outData) const;
        SSTableFile::TimestampType getTimeNow(); // helper to get current timestamp
        std::optional<SSTableFile> read(std::string file) const;

    public:
        SSTableFileManagerImpl(std::string directoryPath);
        std::optional<Error> write(std::vector<const Entry*> entryPtrs) override;
        std::optional<Entry> get(const std::string& key) const override; // searches for a key
        std::optional<SSTableFile::TimestampType> getTimestamp() const override;
};

#endif