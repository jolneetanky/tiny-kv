#include "writebuffer.h"
#include <unordered_map>

WriteBuffer::WriteBuffer(int size) : m_size{size} {};

std::string WriteBuffer::put(const std::string &key, const std::string &val)
{
    // invariant: at any point in time, the buffer contains `n` elements where `n` <= m_size.
    // so before adding we need to first check if buffer is full.
    std::cout << "SIZE BEFORE INSERT: " << m_write_buffer.size() << "\n";
    if (m_write_buffer.size() == m_size)
    {
        flushToDisk();
    }

    m_write_buffer[key] = val;
    std::cout << "SIZE AFTER INSERT: " << m_write_buffer.size() << "\n";
    std::cout << "[WriteBuffer]" << " PUT " << key << ", " << val << "\n";

    return key;
};

// Returns the key if it exists in buffer.
// Else returns empty string.
std::string WriteBuffer::get(const std::string &key)
{
    auto it = m_write_buffer.find(key);
    if (it == m_write_buffer.end())
    {
        std::cout << "[WriteBuffer] Key \"" << key << "\" does not exist in buffer.\n";
        return "";
    }

    std::string val = it->second;
    if (val.empty())
    {
        std::cout << "[WriteBuffer]" << " Key \"" << key << "\" does not exist in buffer." << "\n";
    }
    else
    {

        std::cout << "[WriteBuffer]" << " GOT: " << val << "\n";
    }
    return val;
};

// Deletes the key from the buffer.
// If key exists in buffer, returns the key
// Else returns empty string
std::string WriteBuffer::del(const std::string &key)
{
    // simply delete from buffer.
    std::string val{get(key)};
    if (val.empty())
    {
        std::cout << "[WriteBuffer]" << " " << "Key " << key << " does not exist in buffer" << "\n";
        return "";
    }
    std::cout << "[WriteBuffer]" << "DELETED: (" << key << ", " << val << ")" << "\n";
    m_write_buffer.erase(key);

    return key;
};

void WriteBuffer::flushToDisk()
{
    m_write_buffer.clear();
    std::cout << "SIZE AFTER CLEAR: " << m_write_buffer.size() << "\n";
    std::cout << "WIP" << "\n";
};