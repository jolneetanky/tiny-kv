#ifndef BLOOM_FILTER_MOCK_H
#define BLOOM_FILTER_MOCK_H

#include <vector>
#include "../../src/bloom_filter/bloom_filter.h"

class MockBloomFilter : public BloomFilter {
    private:
        std::vector<bool> m_bit_array;
        size_t m_num_hashes;

    public:
        MockBloomFilter(size_t size, size_t num_hashes);

        void insert(const std::string& item) override;
        bool contains(const std::string& item) const override;

        ~MockBloomFilter() = default;
};

#endif