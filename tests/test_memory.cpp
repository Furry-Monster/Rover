#include "core/os/memory.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>

// ===========================================================================
// Aligned allocation
// ===========================================================================

TEST_CASE("mem_alloc_aligned: basic allocate and free")
{
    void* ptr = mem_alloc_aligned(128, 16);
    REQUIRE(ptr != nullptr);
    std::memset(ptr, 0xAB, 128);
    mem_free_aligned(ptr);
}

TEST_CASE("mem_alloc_aligned: pointer is aligned")
{
    constexpr std::size_t alignments[] = {8, 16, 32, 64, 128, 256};

    for (auto align : alignments)
    {
        void* ptr = mem_alloc_aligned(1024, align);
        REQUIRE(ptr != nullptr);
        CAPTURE(align);
        CHECK((reinterpret_cast<std::uintptr_t>(ptr) % align) == 0);
        mem_free_aligned(ptr);
    }
}

TEST_CASE("mem_alloc_aligned: size not multiple of alignment still works")
{
    void* ptr = mem_alloc_aligned(13, 32);
    REQUIRE(ptr != nullptr);
    std::memset(ptr, 0, 13);
    mem_free_aligned(ptr);
}

TEST_CASE("mem_free_aligned: free nullptr is safe")
{
    mem_free_aligned(nullptr); // must not crash
}

// ===========================================================================
// memnew / memdelete
// ===========================================================================

TEST_CASE("memnew/memdelete: trivial type")
{
    int* p = memnew<int>(42);
    REQUIRE(p != nullptr);
    CHECK(*p == 42);
    memdelete(p);
}

TEST_CASE("memnew/memdelete: class with constructor/destructor")
{
    static int ctor_count = 0;
    static int dtor_count = 0;

    struct Widget
    {
        int value;

        Widget(int v) : value(v) { ++ctor_count; }

        ~Widget() { ++dtor_count; }
    };

    ctor_count = 0;
    dtor_count = 0;

    Widget* w = memnew<Widget>(99);
    CHECK(ctor_count == 1);
    CHECK(w->value == 99);

    memdelete(w);
    CHECK(dtor_count == 1);
}

TEST_CASE("memdelete: nullptr is safe")
{
    memdelete(static_cast<int*>(nullptr)); // must not crash
}
