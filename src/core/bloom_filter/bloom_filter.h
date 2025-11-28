#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <vector>

class BloomFilter
{
public:
    virtual void insert(const std::string &item) = 0;
    virtual bool contains(const std::string &item) const = 0;

    virtual ~BloomFilter() = default;
};

#endif