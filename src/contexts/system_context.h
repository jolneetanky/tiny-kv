#ifndef SYSTEM_CONTEXT
#define SYSTEM_CONTEXT
#include <atomic>
#include <limits>
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

    void restore(FileNumber max_file_num_on_disk)
    {
        const FileNumber next_file_number = max_file_num_on_disk == std::numeric_limits<FileNumber>::max()
                                                ? max_file_num_on_disk
                                                : max_file_num_on_disk + 1;
        FileNumber current = counter.load(std::memory_order_relaxed);
        while (current < next_file_number &&
               !counter.compare_exchange_weak(
                   current,
                   next_file_number,
                   std::memory_order_relaxed,
                   std::memory_order_relaxed))
        {
        }
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
