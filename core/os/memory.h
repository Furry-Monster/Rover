#pragma once

#include <cstddef>
#include <cstdlib>
#include <utility>

#if defined(_MSC_VER)
    #include <malloc.h>
#endif

// Aligned heap allocation — size is rounded up to a multiple of align.
[[nodiscard]] void*
mem_alloc_aligned(std::size_t size, std::size_t align);
void
mem_free_aligned(void* ptr) noexcept;

template <typename T, typename... Args>
[[nodiscard]] T*
memnew(Args&&... args)
{
    return new T(std::forward<Args>(args)...);
}

template <typename T>
void
memdelete(T* ptr) noexcept
{
    delete ptr;
}
