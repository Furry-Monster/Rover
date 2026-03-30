#include "core/string/string_name.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>
#include <unordered_set>

// ===========================================================================
// Construction
// ===========================================================================

TEST_CASE("StringName: default construction yields empty")
{
    StringName sn;
    CHECK(sn.empty());
    CHECK_FALSE(static_cast<bool>(sn));
    CHECK(sn.length() == 0);
    CHECK(sn.hash() == 0u);
    CHECK(std::string(sn.c_str()) == "");
}

TEST_CASE("StringName: construct from const char*")
{
    StringName sn("hello");
    CHECK_FALSE(sn.empty());
    CHECK(static_cast<bool>(sn));
    CHECK(sn.length() == 5);
    CHECK(sn.hash() != 0u);
    CHECK(std::string(sn.c_str()) == "hello");
}

TEST_CASE("StringName: construct from nullptr is empty")
{
    StringName sn(static_cast<const char*>(nullptr));
    CHECK(sn.empty());
}

TEST_CASE("StringName: construct from empty string is empty")
{
    StringName sn("");
    CHECK(sn.empty());
}

TEST_CASE("StringName: construct from std::string")
{
    std::string s = "world";
    StringName  sn(s);
    CHECK(std::string(sn.c_str()) == "world");
    CHECK(sn.length() == 5);
}

TEST_CASE("StringName: construct from empty std::string is empty")
{
    StringName sn(std::string(""));
    CHECK(sn.empty());
}

// ===========================================================================
// Interning — identity guarantee
// ===========================================================================

TEST_CASE("StringName: same string interns to identical pointer")
{
    StringName a("interned");
    StringName b("interned");
    CHECK(a == b);
    CHECK(a.c_str() == b.c_str()); // same Data pointer → same c_str pointer
}

TEST_CASE("StringName: different strings are not equal")
{
    StringName a("alpha");
    StringName b("beta");
    CHECK(a != b);
}

TEST_CASE("StringName: empty names are equal")
{
    StringName a;
    StringName b;
    CHECK(a == b);
}

// ===========================================================================
// Copy & Move
// ===========================================================================

TEST_CASE("StringName: copy construction shares identity")
{
    StringName original("copy_me");
    StringName copy(original);
    CHECK(copy == original);
    CHECK(std::string(copy.c_str()) == "copy_me");
}

TEST_CASE("StringName: move construction transfers ownership")
{
    StringName original("move_me");
    StringName moved(std::move(original));

    CHECK(std::string(moved.c_str()) == "move_me");
    CHECK(original.empty()); // NOLINT — testing post-move state
}

TEST_CASE("StringName: copy assignment")
{
    StringName a("src");
    StringName b;
    b = a;
    CHECK(b == a);
    CHECK(std::string(b.c_str()) == "src");
}

TEST_CASE("StringName: move assignment")
{
    StringName a("src");
    StringName b;
    b = std::move(a);
    CHECK(std::string(b.c_str()) == "src");
    CHECK(a.empty()); // NOLINT — testing post-move state
}

TEST_CASE("StringName: self-assignment is safe")
{
    StringName  a("safe");
    const auto* old_ptr = a.c_str();
    a                   = a; // NOLINT — intentional self-assign
    CHECK(a.c_str() == old_ptr);
    CHECK(std::string(a.c_str()) == "safe");
}

// ===========================================================================
// Hashing & STL compatibility
// ===========================================================================

TEST_CASE("StringName: deterministic hash")
{
    StringName a("hash_test");
    StringName b("hash_test");
    CHECK(a.hash() == b.hash());
}

TEST_CASE("StringName: std::hash specialization works with unordered_set")
{
    std::unordered_set<StringName> set;
    set.insert(StringName("one"));
    set.insert(StringName("two"));
    set.insert(StringName("one")); // duplicate

    CHECK(set.size() == 2);
    CHECK(set.count(StringName("one")) == 1);
    CHECK(set.count(StringName("two")) == 1);
    CHECK(set.count(StringName("three")) == 0);
}

// ===========================================================================
// Operators
// ===========================================================================

TEST_CASE("StringName: operator< provides stable ordering")
{
    StringName a("aaa");
    StringName b("bbb");
    // One must be less than the other (pointer-based, deterministic per run).
    CHECK((a < b) != (b < a));
    CHECK_FALSE(a < a);
}

TEST_CASE("StringName: operator bool")
{
    CHECK_FALSE(static_cast<bool>(StringName()));
    CHECK(static_cast<bool>(StringName("x")));
}
