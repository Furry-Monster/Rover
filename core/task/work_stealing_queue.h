#pragma once

#include <atomic>
#include <cassert>
#include <vector>

#include "core/typedefs.h"

namespace rover {

// Chase-Lev work-stealing deque.
// Owner thread pushes/pops from the bottom (LIFO — cache-hot).
// Other threads steal from the top (FIFO).
// Capacity must be a power of two.
template <typename T>
class WorkStealingQueue {
public:
    explicit WorkStealingQueue(usize capacity)
        : capacity_(capacity), mask_(capacity - 1), buffer_(capacity) {
        assert(capacity > 0 && (capacity & (capacity - 1)) == 0);
    }

    WorkStealingQueue(const WorkStealingQueue&)            = delete;
    WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;

    void push(T item) {
        i64 b = bottom_.load(std::memory_order_relaxed);
        i64 t = top_.load(std::memory_order_acquire);

        if (static_cast<usize>(b - t) >= capacity_) {
            return; // full — drop (caller can fall back to global queue)
        }

        buffer_[static_cast<usize>(b) & mask_] = std::move(item);
        std::atomic_thread_fence(std::memory_order_release);
        bottom_.store(b + 1, std::memory_order_relaxed);
    }

    // Pop from bottom — owner thread only.
    bool try_pop(T& out) {
        i64 b = bottom_.load(std::memory_order_relaxed) - 1;
        bottom_.store(b, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        i64 t = top_.load(std::memory_order_relaxed);

        if (t <= b) {
            out = std::move(buffer_[static_cast<usize>(b) & mask_]);
            if (t == b) {
                // Last element — race with stealers.
                if (!top_.compare_exchange_strong(t, t + 1,
                        std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    bottom_.store(t + 1, std::memory_order_relaxed);
                    return false;
                }
                bottom_.store(t + 1, std::memory_order_relaxed);
            }
            return true;
        }

        // Queue was empty.
        bottom_.store(t, std::memory_order_relaxed);
        return false;
    }

    // Steal from top — any thread.
    bool try_steal(T& out) {
        i64 t = top_.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        i64 b = bottom_.load(std::memory_order_acquire);

        if (t >= b) {
            return false;
        }

        out = buffer_[static_cast<usize>(t) & mask_];
        if (!top_.compare_exchange_strong(t, t + 1,
                std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return false; // lost race with another stealer or owner
        }
        return true;
    }

private:
    usize              capacity_;
    usize              mask_;
    std::vector<T>     buffer_;
    std::atomic<i64>   bottom_{0};
    std::atomic<i64>   top_{0};
};

} // namespace rover
