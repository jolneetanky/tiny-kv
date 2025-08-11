#ifndef MEM_TABLE_H
#define MEM_TABLE_H

#include <iostream>
#include <optional>
#include "../types/error.h"
#include "../types/entry.h"

// internal storage of MemTable is an `Entry`.
class MemTable
{
private:
    int m_size;

public:
    // virtual Error* put(const std::string &key, const std::string &val) = 0;
    virtual std::optional<Error> put(const std::string &key, const std::string &val) = 0;
    virtual std::optional<Entry> get(const std::string &key) const = 0;
    virtual std::optional<Error> del(const std::string &key) = 0;
    virtual ~MemTable() = default;
};

#endif