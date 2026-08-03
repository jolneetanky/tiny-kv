#ifndef SKIP_LIST_H
#define SKIP_LIST_H

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "core/iterators/iterator.h"
#include "types/entry.h"
#include "types/error.h"

class SkipList
{
public:
    virtual std::optional<Error> set(Entry const &entry) = 0;
    virtual std::optional<Entry> get(const std::string &key) const = 0;
    virtual std::optional<std::vector<Entry>> getAll() const = 0;
    virtual std::unique_ptr<tinykv::Iterator> NewIterator() const = 0;
    virtual std::optional<Error> clear() = 0;
    virtual std::optional<int> getLength() const = 0;

    virtual ~SkipList() = default;
};

#endif
