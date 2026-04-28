// Unit tests for core/task/.
//
// Exercise: ThreadSafeQueue MPMC behavior, JobSystem submit/wait, and
// concurrent safety. Tests deliberately use small numbers of jobs to
// stay deterministic; load-stress lives elsewhere.

#include <doctest/doctest.h>

#include "core/task/job_system.h"
#include "core/task/thread_safe_queue.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace rover;

// ---------------------------------------------------------------------------
// ThreadSafeQueue
// ---------------------------------------------------------------------------
TEST_CASE("ThreadSafeQueue: push then try_pop preserves FIFO order") {
    ThreadSafeQueue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);
    CHECK(q.size() == 3);

    int v = -1;
    CHECK(q.try_pop(v)); CHECK(v == 1);
    CHECK(q.try_pop(v)); CHECK(v == 2);
    CHECK(q.try_pop(v)); CHECK(v == 3);
    CHECK_FALSE(q.try_pop(v));
    CHECK(q.is_empty());
}

TEST_CASE("ThreadSafeQueue: shutdown wakes wait_pop") {
    ThreadSafeQueue<int> q;

    std::thread waiter([&]() {
        int v = -1;
        const bool ok = q.wait_pop(v);
        CHECK_FALSE(ok);    // shutdown short-circuits
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    q.shutdown();
    waiter.join();
}

TEST_CASE("ThreadSafeQueue: multi-producer single-consumer round-trip") {
    ThreadSafeQueue<int> q;

    constexpr int kPerProducer = 100;
    constexpr int kProducers   = 4;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&q, p]() {
            for (int i = 0; i < kPerProducer; ++i) {
                q.push(p * kPerProducer + i);
            }
        });
    }
    for (auto& t : producers) t.join();

    long long sum = 0;
    int v = 0;
    while (q.try_pop(v)) {
        sum += v;
    }

    long long expected = 0;
    for (int p = 0; p < kProducers; ++p) {
        for (int i = 0; i < kPerProducer; ++i) {
            expected += p * kPerProducer + i;
        }
    }
    CHECK(sum == expected);
}

// ---------------------------------------------------------------------------
// JobSystem
// ---------------------------------------------------------------------------
TEST_CASE("JobSystem: worker_count uses provided thread_count") {
    JobSystem js{2};
    CHECK(js.worker_count() == 2);
    js.init();
    js.shutdown();
}

TEST_CASE("JobSystem: every submitted job runs exactly once") {
    JobSystem js{2};
    js.init();

    constexpr int kJobs = 200;
    std::atomic<int> counter{0};
    for (int i = 0; i < kJobs; ++i) {
        js.submit([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    js.wait_all();
    CHECK(counter.load() == kJobs);

    js.shutdown();
}

TEST_CASE("JobSystem: submit_with_handle reports completion") {
    JobSystem js{2};
    js.init();

    std::atomic<bool> ran{false};
    JobHandle h = js.submit_with_handle([&ran]() {
        ran.store(true, std::memory_order_release);
    });
    js.wait(h);
    CHECK(h.is_complete());
    CHECK(ran.load());

    js.shutdown();
}

TEST_CASE("JobSystem: many jobs sum the right total") {
    JobSystem js{0};   // auto-detect (defaults to hardware_concurrency - 1)
    js.init();
    REQUIRE(js.worker_count() >= 1);

    std::atomic<long long> sum{0};
    constexpr int kJobs = 500;
    for (int i = 1; i <= kJobs; ++i) {
        js.submit([&sum, i]() {
            sum.fetch_add(i, std::memory_order_relaxed);
        });
    }
    js.wait_all();

    long long expected = static_cast<long long>(kJobs) * (kJobs + 1) / 2;
    CHECK(sum.load() == expected);

    js.shutdown();
}

TEST_CASE("JobSystem: jobs submitted from within a job execute") {
    // This exercises the local-queue path: when a job runs on a worker
    // thread and submits more jobs, those go into the worker's local
    // work-stealing deque rather than the global queue.
    JobSystem js{2};
    js.init();

    std::atomic<int> outer{0};
    std::atomic<int> inner{0};

    constexpr int kOuter = 10;
    constexpr int kInner = 5;
    for (int i = 0; i < kOuter; ++i) {
        js.submit([&]() {
            outer.fetch_add(1, std::memory_order_relaxed);
            for (int j = 0; j < kInner; ++j) {
                js.submit([&]() {
                    inner.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }
    js.wait_all();
    CHECK(outer.load() == kOuter);
    CHECK(inner.load() == kOuter * kInner);

    js.shutdown();
}
