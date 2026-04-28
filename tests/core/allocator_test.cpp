// Unit tests for core/allocator/.
//
// Exercise: bump-and-reset semantics, pool free-list correctness,
// arena cross-chunk allocation. We use cstdint instead of relying on
// the system's "max_align_t" sized alignment to keep assertions tight.

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "core/allocator/arena_allocator.h"
#include "core/allocator/linear_allocator.h"
#include "core/allocator/pool_allocator.h"

using namespace rover;

namespace {

bool is_aligned(const void* p, usize a) noexcept {
    return (reinterpret_cast<std::uintptr_t>(p) % a) == 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// LinearAllocator
// ---------------------------------------------------------------------------
TEST_CASE("LinearAllocator: bumps offset and respects capacity") {
    LinearAllocator alloc{1024};
    CHECK(alloc.capacity() == 1024);
    CHECK(alloc.used()     == 0);

    void* a = alloc.allocate(100, 1);
    CHECK(a != nullptr);
    CHECK(alloc.used() == 100);

    void* b = alloc.allocate(200, 1);
    CHECK(b != nullptr);
    CHECK(b > a);
    CHECK(alloc.used() == 300);
}

TEST_CASE("LinearAllocator: respects requested alignment up to max_align_t") {
    // The underlying buffer comes from make_unique<u8[]>, which is only
    // guaranteed aligned to alignof(std::max_align_t). We test alignments
    // up to that threshold; over-aligned allocations would require the
    // allocator to over-allocate the buffer (a future enhancement).
    constexpr usize MAX = alignof(std::max_align_t);

    LinearAllocator alloc{1024};
    alloc.allocate(1, 1);                       // bump to offset 1
    void* p4  = alloc.allocate(16, 4);
    void* p8  = alloc.allocate(16, 8);
    void* pmx = alloc.allocate(16, MAX);
    CHECK(p4  != nullptr);
    CHECK(p8  != nullptr);
    CHECK(pmx != nullptr);
    CHECK(is_aligned(p4,  4));
    CHECK(is_aligned(p8,  8));
    CHECK(is_aligned(pmx, MAX));
}

TEST_CASE("LinearAllocator: returns nullptr when out of capacity") {
    LinearAllocator alloc{64};
    CHECK(alloc.allocate(60, 1) != nullptr);
    CHECK(alloc.allocate(20, 1) == nullptr);   // 60 + 20 > 64
}

TEST_CASE("LinearAllocator: reset reuses the buffer") {
    LinearAllocator alloc{128};
    void* first = alloc.allocate(64, 1);
    CHECK(first != nullptr);
    CHECK(alloc.used() == 64);

    alloc.reset();
    CHECK(alloc.used() == 0);

    void* second = alloc.allocate(64, 1);
    CHECK(second == first);   // same backing buffer reused
}

TEST_CASE("LinearAllocator: construct<T> placement-news a value") {
    struct Point { int x, y; Point(int a, int b) : x(a), y(b) {} };
    LinearAllocator alloc{256};
    Point* p = alloc.construct<Point>(7, 11);
    REQUIRE(p != nullptr);
    CHECK(p->x == 7);
    CHECK(p->y == 11);
    CHECK(is_aligned(p, alignof(Point)));
}

// ---------------------------------------------------------------------------
// PoolAllocator
// ---------------------------------------------------------------------------
TEST_CASE("PoolAllocator: hands out distinct blocks until exhausted") {
    PoolAllocator pool{32, 4};
    CHECK(pool.capacity()   == 4);
    CHECK(pool.free_count() == 4);

    std::vector<void*> blocks;
    for (int i = 0; i < 4; ++i) {
        void* p = pool.allocate();
        REQUIRE(p != nullptr);
        blocks.push_back(p);
    }
    CHECK(pool.free_count() == 0);
    CHECK(pool.allocate() == nullptr);  // exhausted

    // All distinct.
    for (usize i = 0; i < blocks.size(); ++i) {
        for (usize j = i + 1; j < blocks.size(); ++j) {
            CHECK(blocks[i] != blocks[j]);
        }
    }
}

TEST_CASE("PoolAllocator: deallocate makes a slot reusable") {
    PoolAllocator pool{16, 2};
    void* a = pool.allocate();
    void* b = pool.allocate();
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(pool.allocate() == nullptr);

    pool.deallocate(a);
    CHECK(pool.free_count() == 1);
    void* c = pool.allocate();
    CHECK(c == a);  // LIFO free list: most recent free is first to be reused
    pool.deallocate(b);
    pool.deallocate(c);
    CHECK(pool.free_count() == 2);
}

TEST_CASE("PoolAllocator: enforces minimum block size for free-list pointer") {
    // Even though we asked for 1-byte blocks, the pool must size up to
    // hold a pointer; otherwise free-list nodes would corrupt memory.
    PoolAllocator pool{1, 4};
    void* p = pool.allocate();
    REQUIRE(p != nullptr);
    pool.deallocate(p);
    CHECK(pool.free_count() == 4);
}

// ---------------------------------------------------------------------------
// ArenaAllocator
// ---------------------------------------------------------------------------
TEST_CASE("ArenaAllocator: serves small allocations from one chunk") {
    ArenaAllocator arena{1024};
    void* a = arena.allocate(100, 1);
    void* b = arena.allocate(100, 1);
    CHECK(a != nullptr);
    CHECK(b != nullptr);
    CHECK(b > a);
}

TEST_CASE("ArenaAllocator: spills to a new chunk when current is full") {
    ArenaAllocator arena{128};
    void* a = arena.allocate(100, 1);
    REQUIRE(a != nullptr);
    // Doesn't fit in remaining 28 bytes; arena must add a new chunk.
    void* b = arena.allocate(100, 1);
    REQUIRE(b != nullptr);
    // Pointers must not overlap a's range.
    auto au = reinterpret_cast<std::uintptr_t>(a);
    auto bu = reinterpret_cast<std::uintptr_t>(b);
    bool disjoint = (bu + 100 <= au) || (au + 100 <= bu);
    CHECK(disjoint);
}

TEST_CASE("ArenaAllocator: oversized allocation gets a dedicated chunk") {
    ArenaAllocator arena{64};
    void* big = arena.allocate(4096, 16);
    REQUIRE(big != nullptr);
    CHECK(is_aligned(big, 16));

    // After the oversized allocation the arena is still usable for small
    // requests (fall back to current chunk or create a new one).
    void* small = arena.allocate(8, 1);
    CHECK(small != nullptr);
}

TEST_CASE("ArenaAllocator: reset rewinds without freeing memory") {
    ArenaAllocator arena{128};
    void* first = arena.allocate(64, 1);
    REQUIRE(first != nullptr);

    arena.reset();
    void* second = arena.allocate(64, 1);
    CHECK(second == first);   // same chunk reused
}

TEST_CASE("ArenaAllocator: construct<T> works") {
    struct Tag { int v; explicit Tag(int x) : v(x) {} };
    ArenaAllocator arena{128};
    Tag* t = arena.construct<Tag>(42);
    REQUIRE(t != nullptr);
    CHECK(t->v == 42);
}
