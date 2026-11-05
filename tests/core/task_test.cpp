// Unit tests for core/task/.
//
// Exercise: ThreadSafeQueue MPMC behavior, JobSystem submit/wait, and
// concurrent safety. Tests deliberately use small numbers of jobs to
// stay deterministic; load-stress lives elsewhere.

#include "core/task/job_system.h"
#include "core/task/thread_safe_queue.h"

#include <doctest/doctest.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <thread>
#include <vector>

using namespace rover;

// ---------------------------------------------------------------------------
// ThreadSafeQueue
// ---------------------------------------------------------------------------
TEST_CASE("ThreadSafeQueue: push then try_pop preserves FIFO order")
{
    ThreadSafeQueue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);
    CHECK(q.size() == 3);

    int v = -1;
    CHECK(q.try_pop(v));
    CHECK(v == 1);
    CHECK(q.try_pop(v));
    CHECK(v == 2);
    CHECK(q.try_pop(v));
    CHECK(v == 3);
    CHECK_FALSE(q.try_pop(v));
    CHECK(q.is_empty());
}

TEST_CASE("ThreadSafeQueue: shutdown wakes wait_pop")
{
    ThreadSafeQueue<int> q;

    std::thread waiter([&]() {
        int        v  = -1;
        const bool ok = q.wait_pop(v);
        CHECK_FALSE(ok); // shutdown short-circuits
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    q.shutdown();
    waiter.join();
}

TEST_CASE("ThreadSafeQueue: multi-producer single-consumer round-trip")
{
    ThreadSafeQueue<int> q;

    constexpr int kPerProducer = 100;
    constexpr int kProducers   = 4;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p)
    {
        producers.emplace_back([&q, p]() {
            for (int i = 0; i < kPerProducer; ++i)
            {
                q.push(p * kPerProducer + i);
            }
        });
    }
    for (auto& t : producers)
    {
        t.join();
    }

    long long sum = 0;
    int       v   = 0;
    while (q.try_pop(v))
    {
        sum += v;
    }

    long long expected = 0;
    for (int p = 0; p < kProducers; ++p)
    {
        for (int i = 0; i < kPerProducer; ++i)
        {
            expected += p * kPerProducer + i;
        }
    }
    CHECK(sum == expected);
}

// ---------------------------------------------------------------------------
// JobSystem
// ---------------------------------------------------------------------------
TEST_CASE("JobSystem: worker_count uses provided thread_count")
{
    JobSystem js{2};
    CHECK(js.worker_count() == 2);
    js.init();
    js.shutdown();
}

TEST_CASE("WorkStealingQueue: overflow preserves the task for fallback")
{
    WorkStealingQueue<Job> queue{2};
    int                    executed = 0;
    REQUIRE(queue.push([&] { executed += 1; }));
    REQUIRE(queue.push([&] { executed += 10; }));
    Job overflow = [&] {
        executed += 100;
    };
    CHECK_FALSE(queue.try_push(overflow));
    REQUIRE(static_cast<bool>(overflow));
    overflow();
    Job task;
    REQUIRE(queue.try_pop(task));
    task();
    CHECK(executed == 110);
    REQUIRE(queue.try_steal(task));
    task();
    CHECK(executed == 111);
    CHECK_FALSE(queue.try_pop(task));
    CHECK_FALSE(queue.try_steal(task));
}

TEST_CASE("WorkStealingQueue: owner and thief consume each task exactly once")
{
    WorkStealingQueue<Job> queue{1};
    constexpr int          rounds = 1000;
    std::barrier           sync{2};
    std::atomic<int>       executed{0};
    std::atomic<int>       empty_tasks{0};
    auto                   run = [&](Job& task) {
        if (task)
        {
            task();
        }
        else
        {
            ++empty_tasks;
        }
    };
    std::jthread thief([&] {
        for (int i = 0; i < rounds; ++i)
        {
            sync.arrive_and_wait();
            Job task;
            if (queue.try_steal(task))
            {
                run(task);
            }
            sync.arrive_and_wait();
        }
    });
    for (int i = 0; i < rounds; ++i)
    {
        // Both consumers finished the previous round, so this slot is empty.
        queue.push([&] { ++executed; });
        sync.arrive_and_wait();
        Job task;
        if (queue.try_pop(task))
        {
            run(task);
        }
        sync.arrive_and_wait();
    }
    thief.join();
    CHECK(executed == rounds);
    CHECK(empty_tasks == 0);
}

TEST_CASE("JobSystem: nested submissions beyond local capacity all complete")
{
    JobSystem system{1};
    system.init();
    std::atomic<int> completed{0};
    constexpr int    jobs = 2048;
    system.submit([&] {
        for (int i = 0; i < jobs; ++i)
        {
            system.submit([&] { ++completed; });
        }
    });
    system.wait_all();
    CHECK(completed == jobs);
    system.shutdown();
}

TEST_CASE("JobSystem: every submitted job runs exactly once")
{
    JobSystem js{2};
    js.init();

    constexpr int    kJobs = 200;
    std::atomic<int> counter{0};
    for (int i = 0; i < kJobs; ++i)
    {
        js.submit([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });
    }
    js.wait_all();
    CHECK(counter.load() == kJobs);

    js.shutdown();
}

TEST_CASE("JobSystem: submit_with_handle reports completion")
{
    JobSystem js{2};
    js.init();

    std::atomic<bool> ran{false};
    JobHandle         h = js.submit_with_handle([&ran]() { ran.store(true, std::memory_order_release); });
    js.wait(h);
    CHECK(h.is_complete());
    CHECK(ran.load());

    js.shutdown();
}

TEST_CASE("JobSystem: many jobs sum the right total")
{
    JobSystem js{0}; // auto-detect (defaults to hardware_concurrency - 1)
    js.init();
    REQUIRE(js.worker_count() >= 1);

    std::atomic<long long> sum{0};
    constexpr int          kJobs = 500;
    for (int i = 1; i <= kJobs; ++i)
    {
        js.submit([&sum, i]() { sum.fetch_add(i, std::memory_order_relaxed); });
    }
    js.wait_all();

    long long expected = static_cast<long long>(kJobs) * (kJobs + 1) / 2;
    CHECK(sum.load() == expected);

    js.shutdown();
}

TEST_CASE("JobSystem: jobs submitted from within a job execute")
{
    // This exercises the local-queue path: when a job runs on a worker
    // thread and submits more jobs, those go into the worker's local
    // work-stealing deque rather than the global queue.
    JobSystem js{2};
    js.init();

    std::atomic<int> outer{0};
    std::atomic<int> inner{0};

    constexpr int kOuter = 10;
    constexpr int kInner = 5;
    for (int i = 0; i < kOuter; ++i)
    {
        js.submit([&]() {
            outer.fetch_add(1, std::memory_order_relaxed);
            for (int j = 0; j < kInner; ++j)
            {
                js.submit([&]() { inner.fetch_add(1, std::memory_order_relaxed); });
            }
        });
    }
    js.wait_all();
    CHECK(outer.load() == kOuter);
    CHECK(inner.load() == kOuter * kInner);

    js.shutdown();
}
