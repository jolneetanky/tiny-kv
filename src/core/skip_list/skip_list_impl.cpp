#include "core/skip_list/skip_list_impl.h"
#include "common/log.h"
#include <iterator>

SkipListImpl::Iter::Iter(const std::map<std::string, Entry> *map) : m_map{map}, m_it{map->end()}
{
}

bool SkipListImpl::Iter::Valid() const
{
    return m_it != m_map->end();
}

void SkipListImpl::Iter::SeekToFirst()
{
    m_it = m_map->begin();
}

void SkipListImpl::Iter::SeekToLast()
{
    if (m_map->empty())
    {
        m_it = m_map->end();
        return;
    }

    m_it = std::prev(m_map->end());
}

void SkipListImpl::Iter::Seek(const std::string &target)
{
    m_it = m_map->find(target);
}

void SkipListImpl::Iter::Next()
{
    if (Valid())
    {
        ++m_it;
    }
}

const std::string &SkipListImpl::Iter::Key() const
{
    return m_it->second.key;
}

const std::string &SkipListImpl::Iter::Value() const
{
    return m_it->second.val;
}

bool SkipListImpl::Iter::isTombstone() const
{
    return m_it->second.tombstone;
}

std::optional<Error> SkipListImpl::set(Entry const &entry)
{
    TINYKV_LOG("[SkipListImpl.set()]");
    m_map[entry.key] = entry;
    return std::nullopt;
}

std::optional<Entry> SkipListImpl::get(const std::string &key) const
{
    TINYKV_LOG("[SkipListImpl.get()]");
    auto it = m_map.find(key);

    if (it != m_map.end())
    {
        Entry entry{it->second}; // destroyed once this function returns
        return it->second;
    }

    return std::nullopt;
}

std::optional<std::vector<Entry>> SkipListImpl::getAll() const
{
    TINYKV_LOG("[SkipListImpl.getAll()]");
    std::vector<Entry> res;

    for (const auto &[key, entry] : m_map)
    {
        res.push_back(entry);
    }

    return res;
}

std::unique_ptr<tinykv::Iterator> SkipListImpl::NewIterator() const
{
    return std::make_unique<SkipListImpl::Iter>(&m_map);
}

std::optional<int> SkipListImpl::getLength() const
{
    return m_map.size();
}

std::optional<Error> SkipListImpl::clear()
{
    TINYKV_LOG("[SkipListImpl.clear()]");
    m_map.clear();
    return std::nullopt;
};
