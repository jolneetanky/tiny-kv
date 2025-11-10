#ifndef BLOOM_FILTER_FACTORY_H
#define BLOOM_FILTER_FACTORY_H

#include "core/bloom_filter/bloom_filter.h"
#include "core/bloom_filter/bloom_filter_impl.h"

// Class to instantiate different types of BloomFilter
// handles creation of BF on heap, but is the caller's responsibility to destroy; by using `unique_ptr` we make sure the BF is destoyed once out of scope so all good
struct BloomFilterFactory
{
    // init bloom filter. Do I create one on the heap or something?
    // nope; create on stack and let it be copied into the caller's stack space. Else you need to explicitly handle deletions and stuff
    virtual std::unique_ptr<BloomFilter> build() const = 0;
    virtual ~BloomFilterFactory() = default;
};

struct DefaultBloomFilterFactory : BloomFilterFactory
{
    std::unique_ptr<BloomFilter> build() const override;
    ~DefaultBloomFilterFactory() = default;
};

// struct MockBloomFilterFactory : BloomFilterFactory {

// };

#endif