#ifndef SYSTEM_CONTEXT
#define SYSTEM_CONTEXT
#include "../factories/bloom_filter_factory.h"

// i should pass in the bloom filter here right instead of the factory idek
// different SSTableFileManagers need their own BloomFilters, so we shouldn't pass in a global one.
struct SystemContext {
    const BloomFilterFactory& m_bloom_filter_factory;

    SystemContext(BloomFilterFactory& bff);
    virtual ~SystemContext() = default;
};

// create a mocked one?

#endif