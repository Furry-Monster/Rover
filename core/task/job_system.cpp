#include "core/task/job_system.h"

#include <random>

namespace rover
{

    static thread_local u32 t_worker_index = UINT32_MAX;

    JobSystem::JobSystem(u32 thread_count)
        : thread_count_(thread_count == 0 ? std::max(1u, std::thread::hardware_concurrency() - 1) : thread_count)
    {}

    JobSystem::~JobSystem()
    {
        shutdown();
    }

    void JobSystem::init()
    {
        local_queues_.reserve(thread_count_);
        for (u32 i = 0; i < thread_count_; ++i)
        {
            local_queues_.push_back(std::make_unique<WorkStealingQueue<Job>>(kLocalQueueCapacity));
        }

        workers_.reserve(thread_count_);
        for (u32 i = 0; i < thread_count_; ++i)
        {
            workers_.emplace_back([this, i](std::stop_token st) { worker_loop(i, std::move(st)); });
        }
    }

    void JobSystem::shutdown()
    {
        for (auto& w : workers_)
        {
            w.request_stop();
        }
        global_queue_.shutdown();
        for (auto& w : workers_)
        {
            if (w.joinable())
            {
                w.join();
            }
        }
        workers_.clear();
        local_queues_.clear();
    }

    void JobSystem::submit(Job job)
    {
        jobs_in_flight_.fetch_add(1, std::memory_order_relaxed);

        if (t_worker_index < thread_count_)
        {
            local_queues_[t_worker_index]->push(std::move(job));
        }
        else
        {
            global_queue_.push(std::move(job));
        }
    }

    JobHandle JobSystem::submit_with_handle(Job job)
    {
        JobHandle handle;
        auto      completed = handle.completed_;

        submit([task = std::move(job), flag = std::move(completed)]() {
            task();
            flag->store(true, std::memory_order_release);
        });

        return handle;
    }

    void JobSystem::wait(const JobHandle& handle)
    {
        while (!handle.is_complete())
        {
            if (t_worker_index < thread_count_)
            {
                if (!try_execute_one(t_worker_index))
                {
                    std::this_thread::yield();
                }
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    void JobSystem::wait_all()
    {
        while (jobs_in_flight_.load(std::memory_order_acquire) > 0)
        {
            if (t_worker_index < thread_count_)
            {
                if (!try_execute_one(t_worker_index))
                {
                    std::this_thread::yield();
                }
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    u32 JobSystem::worker_count() const
    {
        return thread_count_;
    }

    void JobSystem::worker_loop(u32 worker_index, std::stop_token stop_token)
    {
        t_worker_index = worker_index;

        while (!stop_token.stop_requested())
        {
            if (!try_execute_one(worker_index))
            {
                // Nothing found anywhere — briefly sleep to avoid busy-spinning.
                Job job;
                // Use a timed wait on the global queue as an efficient idle strategy.
                // The wait_pop will return early on shutdown().
                if (global_queue_.wait_pop(job))
                {
                    job();
                    jobs_in_flight_.fetch_sub(1, std::memory_order_relaxed);
                }
            }
        }

        // Drain remaining work before exiting.
        while (try_execute_one(worker_index))
        {}
    }

    bool JobSystem::try_execute_one(u32 worker_index)
    {
        Job job;

        // 1. Own local queue (LIFO — hot cache).
        if (local_queues_[worker_index]->try_pop(job))
        {
            job();
            jobs_in_flight_.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }

        // 2. Steal from a random other worker.
        thread_local std::minstd_rand rng(std::hash<std::thread::id>{}(std::this_thread::get_id()));

        u32 count = thread_count_;
        u32 start = rng() % count;
        for (u32 i = 0; i < count; ++i)
        {
            u32 idx = (start + i) % count;
            if (idx == worker_index)
            {
                continue;
            }
            if (local_queues_[idx]->try_steal(job))
            {
                job();
                jobs_in_flight_.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }
        }

        // 3. Global overflow queue (non-blocking).
        if (global_queue_.try_pop(job))
        {
            job();
            jobs_in_flight_.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }

        return false;
    }

} // namespace rover
