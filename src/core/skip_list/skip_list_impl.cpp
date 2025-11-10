#include "core/skip_list/skip_list_impl.h"

std::optional<Error> SkipListImpl::set(Entry const &entry)
{
    std::cout << "[SkipListImpl.set()]" << "\n";
    m_map[entry.key] = entry;
    return std::nullopt;
}

std::optional<Entry> SkipListImpl::get(const std::string &key) const
{
    std::cout << "[SkipListImpl.get()]" << std::endl;
    auto it = m_map.find(key);

    if (it != m_map.end())
    {
        Entry entry{it->second}; // destroyed once this function returns
        return it->second;
        // if (entry.tombstone == false) {
        //     return it->second;
        // }
    }

    return std::nullopt;
}

std::optional<std::vector<Entry>> SkipListImpl::getAll() const
{
    std::cout << "SkipListImpl.getAll()" << std::endl;
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
    std::cout << "SkipListImpl.clear()" << std::endl;
    m_map.clear();
    return std::nullopt;
};