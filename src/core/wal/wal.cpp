#include <iostream>
#include <filesystem>
#include <cerrno>
#include <cstring>
#include <fstream>
#include "wal.h"

WAL::WAL(uint64_t id) : m_id{id}
{
    m_fname = _buildFileName(id);
    m_fullPath = BASE_PATH + "/" + m_fname;
};

void _createDirectory(std::string dirPath)
{
    // create parent directories in path if needed
    std::filesystem::path fsPath{dirPath};
    std::filesystem::create_directories(fsPath);
}

std::string WAL::_buildFileName(uint64_t id)
{
    return "wal-" + std::to_string(id);
};

std::optional<Error> WAL::append(const Entry &e)
{
    std::cout << "[WAL.append()]" << "\n";
    // create directory if it doens't exist
    _createDirectory(BASE_PATH);

    std::ofstream out(m_fullPath, std::ios::binary | std::ios::app);
    if (!out)
    {
        return Error{"open WAL for append failed: " + m_fullPath};
    }

    uint8_t type = e.tombstone ? 1u : 0u;
    uint32_t klen = static_cast<uint32_t>(e.key.size());
    uint32_t vlen = e.tombstone ? 0u : static_cast<uint32_t>(e.val.size());

    // write header
    out.write(reinterpret_cast<const char *>(&type), sizeof(type));
    out.write(reinterpret_cast<const char *>(&klen), sizeof(klen));
    out.write(reinterpret_cast<const char *>(&vlen), sizeof(vlen));
    if (!out)
        return Error{"write WAL header failed"};

    // write payloads
    if (klen)
        out.write(e.key.data(), klen);
    if (!e.tombstone && vlen)
        out.write(e.val.data(), vlen);
    if (!out)
        return Error{"write WAL payload failed"};

    out.flush(); // v0: rely on flush; add fsync later if you want stronger durability
    if (!out)
        return Error{"flush WAL failed"};

    return std::nullopt;
};

std::optional<Error> WAL::remove()
{
    std::cout << "WAL.remove()" << "\n";
    try
    {
        if (std::filesystem::exists(m_fullPath))
        {
            std::filesystem::remove(m_fullPath);
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        return Error{std::string{"WAL::remove failed: "} + e.what()};
    }
    return std::nullopt;
};

std::optional<std::vector<Entry>> WAL::getEntries()
{
    std::ifstream in(m_fullPath, std::ios::binary);
    if (!in)
    {
        // No WAL is fine: treat as empty
        return std::vector<Entry>{};
    }

    std::vector<Entry> entries;

    while (true)
    {
        uint8_t type;
        uint32_t klen = 0, vlen = 0;

        // try to read header; if we hit EOF cleanly, break
        in.read(reinterpret_cast<char *>(&type), sizeof(type));
        if (!in)
        {
            if (in.eof())
                break;                                                       // clean end
            std::cout << "[WAL.readEntries()] read WAL type failed" << "\n"; // hard error
            return std::nullopt;
        }

        in.read(reinterpret_cast<char *>(&klen), sizeof(klen));
        if (!in)
            break; // partial tail -> stop replaying

        in.read(reinterpret_cast<char *>(&vlen), sizeof(vlen));
        if (!in)
            break; // partial tail -> stop replaying

        // sanity checks (very light for v0)
        if (klen > (1u << 30) || vlen > (1u << 30))
        {
            std::cout << "[WAL.readEntries()] WAL record lengths look corrupt";
            return std::nullopt;
        }

        std::string key, val;
        key.resize(klen);
        if (klen)
        {
            in.read(key.data(), klen);
            if (!in)
                break; // partial tail
        }

        bool tombstone = (type == 1u);
        if (!tombstone && vlen)
        {
            val.resize(vlen);
            in.read(val.data(), vlen);
            if (!in)
                break; // partial tail
        }

        entries.emplace_back(std::move(key), std::move(val), tombstone);
    }

    return entries;
};