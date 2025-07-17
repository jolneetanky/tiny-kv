#ifndef WRITEBUFFER_IMPL_H
#define WRITEBUFFER_IMPL_H

#include <iostream>
#include "writebuffer.h"
#include "../disk_manager/disk_manager.h"

class WriteBufferImpl : public WriteBuffer
{
private:
    std::unordered_map<std::string, std::string> m_write_buffer;
    DiskManager &m_diskManager;

    // Flushes data from buffer to disk
    void flushToDisk() override;
    // Helper function to serialize data in buffer before flushing to disk
    std::string serializeData();
    std::string serializeData(const std::string &key, const std::string &val);
    std::string serializeData(const std::unordered_map<std::string, std::string> &data);

public:
    WriteBufferImpl(int size, DiskManager &diskManager); // constructor
    void put(const std::string &key, const std::string &val) override;
    std::string get(const std::string &key) const override;
    void del(const std::string &key) override;
};

#endif