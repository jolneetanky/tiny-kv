#include "level_manager_impl.h"

LevelManagerImpl::LevelManagerImpl(int levelNum, std::string directoryPath) : m_levelNum { levelNum }, m_directoryPath { directoryPath } {};


std::optional<Error> LevelManagerImpl::writeFile(std::vector<const Entry*> entries) {};
std::optional<Entry> LevelManagerImpl::searchKey(const std::string &key) {};
std::optional<const SSTableFileManager*> LevelManagerImpl::getFiles() {};
std::optional<Error> LevelManagerImpl::deleteFiles(const SSTableFileManager*) {};