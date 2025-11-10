#include "bloom_filter_factory.h"
#include "../core/bloom_filter/bloom_filter_impl.h"

std::unique_ptr<BloomFilter> DefaultBloomFilterFactory::build() const
{
    // create a BF on heap(?) But when do I destroy it
    // If we create on heap, this class must be resopnsible of keeping track what has been created, and deleting them
    // Better to just pass by value to control is transferred to the caller.
    return std::make_unique<BloomFilterImpl>(BloomFilterImpl(1000, 7));
}