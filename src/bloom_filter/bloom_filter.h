#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <vector>

class BloomFilter {
    private:
        std::vector<bool> m_bit_array;
        size_t m_num_hashes;

    public:
        BloomFilter(size_t size, size_t num_hashes);

        void insert(const std::string& item);
        bool contains(const std::string& item) const;
};

#endif