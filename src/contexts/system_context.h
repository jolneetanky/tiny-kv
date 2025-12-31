#ifndef SYSTEM_CONTEXT
#define SYSTEM_CONTEXT
#include <atomic>
#include "types/types.h"

/*
Thread-safe file number allocator.
*/
class FileNumberAllocator
{
public:
    FileNumber next()
    {
        return counter.fetch_add(1, std::memory_order_relaxed); // compiler is free to reorder operations
    }

    void restore(FileNumber max_fil_num_on_disk)
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