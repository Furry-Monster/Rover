#include "core/allocator/arena_allocator.h"

#include <algorithm>

namespace rover
{

    ArenaAllocator::ArenaAllocator(usize chunk_size) : chunk_size_(chunk_size)
    {
        assert(chunk_size > 0);
    }

    ArenaAllocator::ArenaAllocator(ArenaAllocator&& other) noexcept
        : chunks_(std::move(other.chunks_)), chunk_size_(other.chunk_size_), current_chunk_(other.current_chunk_)
    {
        other.chunk_size_    = 64 * 1024;
        other.current_chunk_ = 0;
    }

    ArenaAllocator& ArenaAllocator::operator=(ArenaAllocator&& other) noexcept
    {
        if (this != &other)
        {
            chunks_              = std::move(other.chunks_);
            chunk_size_          = other.chunk_size_;
            current_chunk_       = other.current_chunk_;
            other.chunk_size_    = 64 * 1024;
            other.current_chunk_ = 0;
        }
        return *this;
    }

    void ArenaAllocator::add_chunk(usize min_size)
    {
        usize size = std::max(chunk_size_, min_size);
        Chunk chunk;
        chunk.buffer = std::make_unique<u8[]>(size);
        chunk.size   = size;
        chunk.offset = 0;
        chunks_.push_back(std::move(chunk));
        current_chunk_ = chunks_.size() - 1;
    }

    void* ArenaAllocator::allocate(usize size, usize alignment)
    {
        assert(size > 0);
        assert((alignment & (alignment - 1)) == 0 && "alignment must be a power of two");

        // Try to allocate from existing chunks starting at current_chunk_
        for (usize i = current_chunk_; i < chunks_.size(); ++i)
        {
            Chunk& chunk          = chunks_[i];
            usize  aligned_offset = (chunk.offset + alignment - 1) & ~(alignment - 1);
            if (aligned_offset + size <= chunk.size)
            {
                void* ptr      = chunk.buffer.get() + aligned_offset;
                chunk.offset   = aligned_offset + size;
                current_chunk_ = i;
                return ptr;
            }
        }

        add_chunk(size + alignment - 1);
        Chunk& chunk          = chunks_.back();
        usize  aligned_offset = (chunk.offset + alignment - 1) & ~(alignment - 1);
        void*  ptr            = chunk.buffer.get() + aligned_offset;
        chunk.offset          = aligned_offset + size;
        return ptr;
    }

    void ArenaAllocator::reset()
    {
        for (auto& chunk : chunks_)
        {
            chunk.offset = 0;
        }
        current_chunk_ = 0;
    }

    void ArenaAllocator::release()
    {
        chunks_.clear();
        current_chunk_ = 0;
    }

} // namespace rover
