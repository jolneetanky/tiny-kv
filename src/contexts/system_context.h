#ifndef SYSTEM_CONTEXT
#define SYSTEM_CONTEXT
#include <atomic>

/*
Thread-safe file number allocator.
*/
class FileNumberAllocator
{
public:
    uint64_t next()
    {
        return counter.fetch_add(1, std::memory_order_relaxed); // compiler is free to reorder operations
    }

    void restore(uint64_t max_fil_num_on_disk)
    {
        counter.store(max_fil_num_on_disk);
    }

private:
    std::atomic<uint64_t> counter{0};
};

struct SystemContext
{
    FileNumberAllocator file_number_allocator; // no need for smart pointer here, that's additional overhead
    virtual ~SystemContext() = default;
};

#endif