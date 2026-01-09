#include "core/skip_list/skip_list_impl.h"
#include "common/log.h"

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