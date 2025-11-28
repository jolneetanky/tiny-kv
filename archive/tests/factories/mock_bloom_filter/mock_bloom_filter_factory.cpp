#include "./mock_bloom_filter_factory.h"
#include "../../mocks/bloom_filter.h"

std::unique_ptr<BloomFilter> MockBloomFilterFactory::build() const {
    return std::make_unique<MockBloomFilter>(7, 1000);
}