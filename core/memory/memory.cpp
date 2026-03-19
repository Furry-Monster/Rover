#include "core/memory/memory.h"

#include <cstdlib>

void*
mem_alloc_aligned(std::size_t size, std::size_t align)
{
#if defined(_MSC_VER)
    return _aligned_malloc(size, align);
#else
    // std::aligned_alloc requires size to be a multiple of align.
    std::size_t aligned_size = (size + align - 1u) & ~(align - 1u);
    return std::aligned_alloc(align, aligned_size);
#endif
}

void
mem_free_aligned(void* ptr) noexcept
{
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}
