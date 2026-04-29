#pragma once

#include "core/typedefs.h"

#include <cassert>
#include <memory>

namespace rover
{

    // A fixed-size block pool allocator using an embedded free list.
    // NOT thread-safe — caller must synchronize externally if shared across threads.
    class PoolAllocator
    {
    public:
        PoolAllocator(usize block_size, usize block_count);
        ~PoolAllocator() = default;

        PoolAllocator(const PoolAllocator&)            = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        PoolAllocator(PoolAllocator&& other) noexcept;
        PoolAllocator& operator=(PoolAllocator&& other) noexcept;

        void* allocate();
        void  deallocate(void* ptr);

        usize free_count() const { return free_count_; }

        usize capacity() const { return block_count_; }

    private:
        struct FreeNode
        {
            FreeNode* next;
        };

        void build_free_list();

        std::unique_ptr<u8[]> buffer_;
        usize                 block_size_  = 0;
        usize                 block_count_ = 0;
        usize                 free_count_  = 0;
        FreeNode*             free_head_   = nullptr;
    };

} // namespace rover
