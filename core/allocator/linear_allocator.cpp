#include "core/allocator/linear_allocator.h"

namespace rover
{

    LinearAllocator::LinearAllocator(usize capacity) : buffer_(std::make_unique<u8[]>(capacity)), capacity_(capacity) {}

    LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
        : buffer_(std::move(other.buffer_)), capacity_(other.capacity_), offset_(other.offset_)
    {
        other.capacity_ = 0;
        other.offset_   = 0;
    }

    LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept
    {
        if (this != &other)
        {
            buffer_         = std::move(other.buffer_);
            capacity_       = other.capacity_;
            offset_         = other.offset_;
            other.capacity_ = 0;
            other.offset_   = 0;
        }
        return *this;
    }

    void* LinearAllocator::allocate(usize size, usize alignment)
    {
        assert(size > 0);
        assert((alignment & (alignment - 1)) == 0 && "alignment must be a power of two");

        usize aligned_offset = (offset_ + alignment - 1) & ~(alignment - 1);
        if (aligned_offset + size > capacity_)
        {
            return nullptr;
        }

        void* ptr = buffer_.get() + aligned_offset;
        offset_   = aligned_offset + size;
        return ptr;
    }

    void LinearAllocator::reset()
    {
        offset_ = 0;
    }

} // namespace rover
