#pragma once

#include "core/typedefs.h"

#include <cassert>
#include <memory>
#include <new>
#include <utility>

namespace rover
{

    // A bump/linear allocator for per-frame temporary allocations.
    // NOT thread-safe — caller must synchronize externally if shared across threads.
    class LinearAllocator
    {
    public:
        explicit LinearAllocator(usize capacity);
        ~LinearAllocator() = default;

        LinearAllocator(const LinearAllocator&)            = delete;
        LinearAllocator& operator=(const LinearAllocator&) = delete;

        LinearAllocator(LinearAllocator&& other) noexcept;
        LinearAllocator& operator=(LinearAllocator&& other) noexcept;

        void* allocate(usize size, usize alignment = alignof(std::max_align_t));
        void  reset();

        template <typename T, typename... Args>
        T* construct(Args&&... args)
        {
            void* mem = allocate(sizeof(T), alignof(T));
            if (!mem)
            {
                return nullptr;
            }
            return new (mem) T(std::forward<Args>(args)...);
        }

        usize used() const { return offset_; }

        usize capacity() const { return capacity_; }

    private:
        std::unique_ptr<u8[]> buffer_;
        usize                 capacity_ = 0;
        usize                 offset_   = 0;
    };

} // namespace rover
