#pragma once

#include <atomic>
#include <memory>
#include <thread>

namespace rover
{

    class JobHandle
    {
    public:
        JobHandle() : completed_(std::make_shared<std::atomic<bool>>(false)) {}

        bool is_complete() const { return completed_->load(std::memory_order_acquire); }

        void wait() const
        {
            while (!completed_->load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
        }

    private:
        friend class JobSystem;

        std::shared_ptr<std::atomic<bool>> completed_;

        void mark_complete() { completed_->store(true, std::memory_order_release); }
    };

} // namespace rover
