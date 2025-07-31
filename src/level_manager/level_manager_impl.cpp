#include "level_manager_impl.h"
#include "../sstable_file_manager/sstable_file_manager_impl.h"
#include <iostream>

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
std::optional<Entry> LevelManagerImpl::searchKey(const std::string &key) {};
std::optional<const SSTableFileManager*> LevelManagerImpl::getFiles() {};
std::optional<Error> LevelManagerImpl::deleteFiles(const SSTableFileManager*) {};