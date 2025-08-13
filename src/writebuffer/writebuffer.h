#ifndef WRITEBUFFER_H
#define WRITEBUFFER_H

#include <iostream>

// invariant: at any point in time, the buffer contains `n` elements where `n` < m_size.
class WriteBuffer
{
protected:
    int m_size;

public:
    WriteBuffer(int size);
    virtual void put(const std::string &key, const std::string &val) = 0;
    virtual std::string get(const std::string &key) const = 0;
    virtual void del(const std::string &key) = 0;
    virtual ~WriteBuffer() = default;
    virtual void flushToDisk() = 0;
};

#endif