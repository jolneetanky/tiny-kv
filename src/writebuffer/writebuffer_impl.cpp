#include "writebuffer_impl.h"
#include <unordered_map>

// WriteBuffer::WriteBuffer(int size) : m_size{size} {};
WriteBufferImpl::WriteBufferImpl(int size, DiskManager &diskManager) : WriteBuffer(size), m_diskManager{diskManager} {};

// PRIVATE FUNCTIONS
// serializes a key value pair.
std::string WriteBufferImpl::serializeData(const std::unordered_map<std::string, std::string> &data)
{
    std::string res;

    for (const auto &[key, val] : m_write_buffer)
    {
        res += std::to_string(key.size()) + ":" + key;
        res += ":" + val;
        res += "\n";
    }

    return res;
}

std::string WriteBufferImpl::serializeData(const std::string &key, const std::string &val)
{
    std::string res;
    res += std::to_string(key.size()) + ":" + key;
    res += ":" + val;
    res += "\n";
    return res;
}

void WriteBufferImpl::flushToDisk()
{
    for (const auto &[key, val] : m_write_buffer)
    {
        m_diskManager.write(serializeData(key, val));
    }
    // write contents of buffer to disk
    // m_diskManager.write(serializeData(m_write_buffer));

    // cleear buffer contents
    m_write_buffer.clear();
    std::cout << "SIZE AFTER CLEAR: " << m_write_buffer.size() << "\n";
};

// PUBLIC FUNCTIONS
void WriteBufferImpl::put(const std::string &key, const std::string &val)
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
};

// Returns the key if it exists in buffer.
// Else returns empty string.
std::string WriteBufferImpl::get(const std::string &key) const
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
void WriteBufferImpl::del(const std::string &key)
{
    // simply delete from buffer.
    std::string val{get(key)};
    if (val.empty())
    {
        std::cout << "[WriteBuffer]" << " " << "Key " << key << " does not exist in buffer" << "\n";
    }
    std::cout << "[WriteBuffer]" << "DELETED: (" << key << ", " << val << ")" << "\n";
    m_write_buffer.erase(key);
    // remove from disk
    m_diskManager.del(key);
};
