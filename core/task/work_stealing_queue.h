#pragma once

#include "core/typedefs.h"

#include <cassert>
#include <mutex>
#include <utility>
#include <vector>

namespace rover
{

    // Bounded work-stealing deque. Owners pop LIFO; thieves steal FIFO.
    // Slot ownership is protected by a mutex because T may be a non-trivial,
    // move-sensitive object such as std::function.
    template <typename T>
    class WorkStealingQueue
    {
    public:
        explicit WorkStealingQueue(usize capacity) : capacity_(capacity), mask_(capacity - 1), buffer_(capacity)
        {
            assert(capacity > 0 && (capacity & (capacity - 1)) == 0);
        }

        WorkStealingQueue(const WorkStealingQueue&)            = delete;
        WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;

        bool push(T item) { return try_push(item); }

        // On overflow item remains intact so the caller can use another queue.
        bool try_push(T& item)
        {
            std::lock_guard lock(mutex_);
            if (size_ == capacity_)
            {
                return false;
            }
            buffer_[(top_ + size_) & mask_] = std::move(item);
            ++size_;
            return true;
        }

        bool try_pop(T& out)
        {
            std::lock_guard lock(mutex_);
            if (size_ == 0)
            {
                return false;
            }
            --size_;
            out = std::move(buffer_[(top_ + size_) & mask_]);
            return true;
        }

        bool try_steal(T& out)
        {
            std::lock_guard lock(mutex_);
            if (size_ == 0)
            {
                return false;
            }
            out  = std::move(buffer_[top_]);
            top_ = (top_ + 1) & mask_;
            --size_;
            return true;
        }

    private:
        usize          capacity_;
        usize          mask_;
        std::vector<T> buffer_;
        usize          top_  = 0;
        usize          size_ = 0;
        std::mutex     mutex_;
    };

} // namespace rover
