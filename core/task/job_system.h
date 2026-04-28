#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "core/typedefs.h"
#include "core/task/job_handle.h"
#include "core/task/thread_safe_queue.h"
#include "core/task/work_stealing_queue.h"

namespace rover {

using Job = std::function<void()>;

class JobSystem {
public:
    explicit JobSystem(u32 thread_count = 0);
    ~JobSystem();

    JobSystem(const JobSystem&)            = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    JobSystem(JobSystem&&)                 = delete;
    JobSystem& operator=(JobSystem&&)      = delete;

    void init();
    void shutdown();

    void      submit(Job job);
    JobHandle submit_with_handle(Job job);

    void wait(const JobHandle& handle);
    void wait_all();

    u32 worker_count() const;

private:
    static constexpr usize kLocalQueueCapacity = 1024;

    void worker_loop(u32 worker_index, std::stop_token stop_token);
    bool try_execute_one(u32 worker_index);

    u32                                                   thread_count_;
    std::vector<std::jthread>                             workers_;
    std::vector<std::unique_ptr<WorkStealingQueue<Job>>>  local_queues_;
    ThreadSafeQueue<Job>                                  global_queue_;
    std::atomic<u64>                                      jobs_in_flight_{0};
};

} // namespace rover
