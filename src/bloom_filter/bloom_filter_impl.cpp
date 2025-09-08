#include "bloom_filter_impl.h"
#include <functional> // for std::hash
#include <iostream>

BloomFilterImpl::BloomFilterImpl(size_t size, size_t num_hashes) : m_bit_array(size, false), m_num_hashes{ num_hashes } {};

void BloomFilterImpl::insert(const std::string& item) {
    for (size_t i = 0; i < m_num_hashes; ++i) {
        size_t hash_val = std::hash<std::string>{}(item + std::to_string(i)); // Simple hash variation
        m_bit_array[hash_val % m_bit_array.size()] = true; // if not there, this won't be true.
    }
};

bool BloomFilterImpl::contains(const std::string& item) const {
    for (size_t i = 0; i < m_num_hashes; ++i) {
        size_t hash_val = std::hash<std::string>{}(item + std::to_string(i));
        if (!m_bit_array[hash_val % m_bit_array.size()]) {
            std::cout << "[BloomFilter.contains()] NOPE" << "\n";
            return false;
        }
    }
    std::cout << "[BloomFilter.contains()] MAYBE IN HERE" << "\n";
    return true;
};