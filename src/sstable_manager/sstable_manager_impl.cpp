#include "sstable_manager_impl.h"
#include "../sstable_file_manager/sstable_file_manager_impl.h"
#include <string>
#include <iostream>

std::vector<SSTableFileManager*> SSTableManagerImpl::getFilesFromDirectory(const std::string &dirName) {
    std::vector<SSTableFileManager*> files;
    for (auto& uptr : m_ssTableFileManagers) {
        files.push_back(uptr.get());
    }
    return files;
}

// write all entries into a file (serialize each entry)
std::optional<Error> SSTableManagerImpl::write(std::vector<const Entry*> entries) {
    std::cout << "[SSTableManagerImpl.write()]" << std::endl;

    m_ssTableFileManagers.push_back(std::make_unique<SSTableFileManagerImpl>(LEVEL_0_DIRECTORY));

    // Get a reference to the newly added manager
    SSTableFileManager* newManager = m_ssTableFileManagers.back().get();
    
    std::optional<Error> errOpt { newManager->write(entries) };
    if (errOpt) {
        std::cerr << "[SSTableManagerImpl.write()] Failed to write to SSTable";
        return errOpt;
    }

    return std::nullopt;

};

std::optional<Entry> SSTableManagerImpl::get(const std::string& key) {
    std::cout << "[SSTableManagerImpl.get()]" << "\n";

    // LOCK DIRECTORY ()
    // NOTE: this is a copy of pointers, modifying it in-place wont change the order of ptrs in ssTablefileManagers
    std::vector<SSTableFileManager*> level0Files { getFilesFromDirectory(LEVEL_0_DIRECTORY)}; // makes a copy

    std::sort(level0Files.begin(), level0Files.end(),
        [](const SSTableFileManager* a, const SSTableFileManager* b) {
            auto ta = a->getTimestamp();
            auto tb = b->getTimestamp();

            // If both have timestamp, compare their values
            // Newer timestamps (with larger values) come first
            if (ta && tb) {
                return ta.value() > tb.value();
            }

            // If only a has timestamp, it is older 
            if (ta && !tb) return true;

            // If only b has timestamp, it should come after a
            if (!ta && tb) return false;

            // If neither has timestamp, consider equal
            return false;
        }
    );

    for (const auto& fileManager : level0Files) {
        // now search in order
        // if key not found, move on to next file
        std::optional<Entry> entryOpt { fileManager->get(key) };
        if (entryOpt && !entryOpt->tombstone) {
            std::cout << "[SSTableManager.get()] FOUND" << "\n";
            return entryOpt;
        } 

        if (entryOpt && entryOpt->tombstone) {
            // key has been deleted
            break;
        }
    }

    std::cout << "[SSTableManager.get()] key does nto exist on disk" << "\n";

    return std::nullopt;
}