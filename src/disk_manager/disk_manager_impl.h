// This class handles writes to disk, and reads from disk.
#ifndef DISK_MANAGER_IMPL_H
#define DISK_MANAGER_IMPL_H

#include <iostream>
#include "disk_manager.h"

// TODO: make this into an interface or something
// This will be a virtual class
class DiskManagerImpl : public DiskManager
{
public:
    DiskManagerImpl(std::string filename);
    void write(const std::string &serializedData) override;

    std::string getKey(const std::string &key) const override;

    void del(const std::string &key) override;
};

#endif