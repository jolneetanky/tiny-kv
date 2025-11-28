#ifndef MOCK_BLOOM_FILTER_FACTORY_H
#define MOCK_BLOOM_FILTER_FACTORY_H

#include "../../../src/factories/bloom_filter_factory.h"

// Creates a mock bloom filter lol
struct MockBloomFilterFactory : BloomFilterFactory {
    std::unique_ptr<BloomFilter> build() const override;
    ~MockBloomFilterFactory() = default;
};

#endif