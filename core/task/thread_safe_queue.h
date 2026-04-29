#pragma once

#include "core/typedefs.h"

#include <condition_variable>
#include <mutex>
#include <queue>

namespace rover
{

    template <typename T>
    class ThreadSafeQueue
    {
    public:
        void push(T item)
        {
            {
                std::lock_guard lock(mutex_);
                queue_.push(std::move(item));
            }
            cv_.notify_one();
        }

        bool try_pop(T& out)
        {
            std::lock_guard lock(mutex_);
            if (queue_.empty())
            {
                return false;
            }
            out = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        bool wait_pop(T& out)
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
            if (shutdown_ && queue_.empty())
            {
                return false;
            }
            out = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        void shutdown()
        {
            {
                std::lock_guard lock(mutex_);
                shutdown_ = true;
            }
            cv_.notify_all();
        }

        bool is_empty() const
        {
            std::lock_guard lock(mutex_);
            return queue_.empty();
        }

        usize size() const
        {
            std::lock_guard lock(mutex_);
            return queue_.size();
        }

    private:
        std::queue<T>           queue_;
        mutable std::mutex      mutex_;
        std::condition_variable cv_;
        bool                    shutdown_ = false;
    };

} // namespace rover
