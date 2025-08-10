#include "level_manager_impl.h"
#include "../sstable_file_manager/sstable_file_manager_impl.h"
#include <iostream>
#include <filesystem>

LevelManagerImpl::LevelManagerImpl(int levelNum, std::string directoryPath) : m_levelNum { levelNum }, m_directoryPath { directoryPath } {};

const int &LevelManagerImpl::getLevel() {
    return m_levelNum;
}

std::optional<Error> LevelManagerImpl::writeFile(std::vector<const Entry*> entries) {
    std::cout << "[LevelManagerImpl.writeFile()]" << "\n";

    m_ssTableFileManagers.push_back(std::make_unique<SSTableFileManagerImpl>(m_directoryPath));

    // Get a reference to the newly added manager
    SSTableFileManager* newManager = m_ssTableFileManagers.back().get();
    
    std::optional<Error> errOpt { newManager->write(entries) };
    if (errOpt) {
        std::cerr << "[SSTableManagerImpl.write()] Failed to write to SSTable";
        return errOpt;
    }

    return std::nullopt;
};

std::optional<Entry> LevelManagerImpl::searchKey(const std::string &key) {
    std::cout << "[LevelManagerImpl.searchKey()] LEVEL " << std::to_string(m_levelNum) << "\n";

    // only need sort if level 0 (other levels don't have overlapping key ranges so it's ok)
    // sort by timestamp -> search L0 SSTables from newest to oldest
    if (m_levelNum == 0) {
        std::sort(m_ssTableFileManagers.begin(), m_ssTableFileManagers.end(),
            [](const std::unique_ptr<SSTableFileManager> &a, const std::unique_ptr<SSTableFileManager> &b) {
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
    }

    for (const auto& fileManager : m_ssTableFileManagers) {
        // now search in order
        // if key not found, move on to next file
        std::optional<Entry> entryOpt { fileManager->get(key) };
        if (entryOpt) {
            std::cout << "[LevelManagerImpl.searchKey()] FOUND" << "\n";
            return entryOpt;
        } 

        // in the latest entry, key has been deleted. So can stop searching alr
        if (entryOpt && entryOpt->tombstone) {
            break;
        }
    }

    std::cout << "[LevelManagerImpl.searchKey()] key does not exist on disk" << "\n";
    return std::nullopt;
};

// std::optional<std::vector<const SSTableFileManager*>> LevelManagerImpl::getFiles() {
// };

std::pair<LevelManagerImpl::const_iterator, LevelManagerImpl::const_iterator> LevelManagerImpl::getFiles() {
    std::cout << "LevelManagerImpl.getFiles()]" << "\n";
    return { m_ssTableFileManagers.begin(), m_ssTableFileManagers.end() };
};

std::optional<Error> LevelManagerImpl::deleteFiles(std::vector<const SSTableFileManager*> files) {
    std::cout << "[LevelManagerImpl.deleteFiles()]" << "\n";

   for (const auto &file : files) {
    std::string fullPath = file->getFullPath();
   
    // 1. Delete file from disk
    if (!std::filesystem::remove(fullPath)) {
        return Error{ "Failed to delete file" };
    }

    // 2. Remove the file from m_fileManagers
    auto it = std::remove_if(
        m_ssTableFileManagers.begin(),
        m_ssTableFileManagers.end(),
        [&](const std::unique_ptr<SSTableFileManager> &ptr) {
            // remove if path of file matches that that we are tryna remove
            return ptr && ptr->getFullPath() == fullPath;
        }
    );

    if (it != m_ssTableFileManagers.end()) {
        m_ssTableFileManagers.erase(it, m_ssTableFileManagers.end());
    }
   } 

   return std::nullopt;
};


std::optional<Error> LevelManagerImpl::init() {
    std::cout << "[LevelManagerImpl.init()]" << "\n";

    // create directory if it doesn't exist
    if (!std::filesystem::exists(m_directoryPath)) {
        try {
            if (!std::filesystem::create_directories(m_directoryPath)) {
                return Error{ "Failed to create directory: " + m_directoryPath };
            }
            std::cout << "[LevelManagerImpl.init()] Created directory: " << m_directoryPath << "\n";
        } catch (const std::filesystem::filesystem_error &e) {
            return Error{ std::string("Filesystem error creating directory: ") + e.what() };
        }
    }

    for (auto const &dirEntry : std::filesystem::directory_iterator{m_directoryPath}) {
        const std::string &fileName = dirEntry.path().filename().string();
        auto fileManager = std::make_unique<SSTableFileManagerImpl>(m_directoryPath, fileName);
        
        if (const auto &err = fileManager->init()) {
            return err;
        }

        m_ssTableFileManagers.push_back(std::move(fileManager));
    }

    return std::nullopt;
 // scans through `m_directoryPath`
 // if there are any files, create an SSTableFileManager to represent the corresponding file.
};