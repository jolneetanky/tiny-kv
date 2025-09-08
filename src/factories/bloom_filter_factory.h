#ifndef BLOOM_FILTER_FACTORY_H
#define BLOOM_FILTER_FACTORY_H

#include "../bloom_filter/bloom_filter.h"
#include "../bloom_filter/bloom_filter_impl.h"

// Class to instantiate different types of BloomFilter
struct BloomFilterFactory {
    // init bloom filter. Do I create one on the heap or something?
    // nope; create on stack and let it be copied into the caller's stack space. Else you need to explicitly handle deletions and stuff
    virtual std::unique_ptr<BloomFilter> build() const = 0;
    virtual ~BloomFilterFactory() = default;
};

struct DefaultBloomFilterFactory : BloomFilterFactory {
    std::unique_ptr<BloomFilter> build() const override;
    ~DefaultBloomFilterFactory() = default;
};

// struct MockBloomFilterFactory : BloomFilterFactory {

// };

#endif