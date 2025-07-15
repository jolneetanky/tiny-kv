#ifndef WRITEBUFFER_H
#define WRITEBUFFER_H

#include <iostream>

// DbImpl inherits from the virtual class DB
class WriteBuffer
{
private:
    std::unordered_map<std::string, std::string> m_write_buffer;
    int m_size;

    void flushToDisk();

public:
    WriteBuffer(int size); // constructor
    std::string put(const std::string &key, const std::string &val);
    std::string get(const std::string &key);
    std::string del(const std::string &key);
};

#endif