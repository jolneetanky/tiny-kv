#ifndef BLOOM_FILTER_IMPL_H
#define BLOOM_FILTER_IMPL_H

#include <vector>
#include "bloom_filter.h"

class BloomFilterImpl : public BloomFilter {
    private:
        std::vector<bool> m_bit_array;
        size_t m_num_hashes;

    public:
        BloomFilterImpl(size_t size, size_t num_hashes);

        void insert(const std::string& item) override;
        bool contains(const std::string& item) const override;
};

#endif