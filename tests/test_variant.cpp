#include "core/variant/variant.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// ===========================================================================
// Construction & type
// ===========================================================================
TEST_CASE("Variant: default is NIL")
{
    Variant v;
    CHECK(v.get_type() == Variant::NIL);
    CHECK(v.is_nil());
}

TEST_CASE("Variant: bool")
{
    Variant t(true);
    Variant f(false);

    CHECK(t.get_type() == Variant::BOOL);
    CHECK(t.as_bool() == true);
    CHECK(f.as_bool() == false);
}

TEST_CASE("Variant: int")
{
    Variant v(int64_t(42));
    CHECK(v.get_type() == Variant::INT);
    CHECK(v.as_int() == 42);
}

TEST_CASE("Variant: float")
{
    Variant v(3.14);
    CHECK(v.get_type() == Variant::FLOAT);
    CHECK(v.as_float() == doctest::Approx(3.14));
}

TEST_CASE("Variant: string")
{
    Variant v("hello");
    CHECK(v.get_type() == Variant::STRING);
    CHECK(v.as_string() == "hello");
}

TEST_CASE("Variant: Vector2")
{
    Variant v(Vector2(1, 2));
    CHECK(v.get_type() == Variant::VECTOR2);

    Vector2 out = v.as_vector2();
    CHECK(out.x == doctest::Approx(1));
    CHECK(out.y == doctest::Approx(2));
}

TEST_CASE("Variant: Vector3")
{
    Variant v(Vector3(1, 2, 3));
    CHECK(v.get_type() == Variant::VECTOR3);

    Vector3 out = v.as_vector3();
    CHECK(out.x == doctest::Approx(1));
    CHECK(out.y == doctest::Approx(2));
    CHECK(out.z == doctest::Approx(3));
}

TEST_CASE("Variant: Vector4")
{
    Variant v(Vector4(1, 2, 3, 4));
    CHECK(v.get_type() == Variant::VECTOR4);

    Vector4 out = v.as_vector4();
    CHECK(out.x == doctest::Approx(1));
    CHECK(out.w == doctest::Approx(4));
}

TEST_CASE("Variant: Color")
{
    Variant v(Color(1, 0.5f, 0, 1));
    CHECK(v.get_type() == Variant::COLOR);

    Color c = v.as_color();
    CHECK(c.r == doctest::Approx(1));
    CHECK(c.g == doctest::Approx(0.5));
}

// ===========================================================================
// Numeric conversions
// ===========================================================================
TEST_CASE("Variant: int/float cross-conversion")
{
    Variant i(int64_t(7));
    CHECK(i.as_float() == doctest::Approx(7.0));

    Variant f(3.9);
    CHECK(f.as_int() == 3);
}

TEST_CASE("Variant: bool conversion")
{
    Variant i(int64_t(0));
    CHECK(i.as_bool() == false);

    Variant i2(int64_t(1));
    CHECK(i2.as_bool() == true);
}

// ===========================================================================
// Comparison
// ===========================================================================
TEST_CASE("Variant: NIL == NIL")
{
    Variant a;
    Variant b;
    CHECK(a == b);
}

TEST_CASE("Variant: same type equality")
{
    CHECK(Variant(42) == Variant(42));
    CHECK(Variant("hello") == Variant("hello"));
    CHECK(Variant(Vector3(1, 2, 3)) == Variant(Vector3(1, 2, 3)));
}

TEST_CASE("Variant: different type inequality")
{
    CHECK(Variant(42) != Variant("42"));
    CHECK(Variant(true) != Variant(1));
}

TEST_CASE("Variant: int == float cross-comparison")
{
    CHECK(Variant(int64_t(5)) == Variant(5.0));
}

// ===========================================================================
// Copy / Move
// ===========================================================================
TEST_CASE("Variant: copy")
{
    Variant original("test string");
    Variant copy = original;

    CHECK(copy.get_type() == Variant::STRING);
    CHECK(copy.as_string() == "test string");
    CHECK(copy == original);
}

TEST_CASE("Variant: move")
{
    Variant original("move me");
    Variant moved = std::move(original);

    CHECK(moved.as_string() == "move me");
    CHECK(original.is_nil());
}

// ===========================================================================
// operator bool
// ===========================================================================
TEST_CASE("Variant: operator bool")
{
    CHECK_FALSE(static_cast<bool>(Variant()));
    CHECK(static_cast<bool>(Variant(true)));
    CHECK_FALSE(static_cast<bool>(Variant(false)));
    CHECK(static_cast<bool>(Variant(int64_t(1))));
    CHECK_FALSE(static_cast<bool>(Variant(int64_t(0))));
    CHECK(static_cast<bool>(Variant("non-empty")));
    CHECK_FALSE(static_cast<bool>(Variant("")));
}

// ===========================================================================
// as_string conversion
// ===========================================================================
TEST_CASE("Variant: as_string for various types")
{
    CHECK(Variant().as_string() == "null");
    CHECK(Variant(true).as_string() == "true");
    CHECK(Variant(false).as_string() == "false");
    CHECK(Variant(int64_t(42)).as_string() == "42");
}

// ===========================================================================
// Hash
// ===========================================================================
TEST_CASE("Variant: hash consistency")
{
    Variant a(int64_t(42));
    Variant b(int64_t(42));
    CHECK(a.hash() == b.hash());

    Variant s1("hello");
    Variant s2("hello");
    CHECK(s1.hash() == s2.hash());
}

TEST_CASE("Variant: type names")
{
    CHECK(std::string(Variant::get_type_name(Variant::NIL)) == "Nil");
    CHECK(std::string(Variant::get_type_name(Variant::STRING)) == "String");
    CHECK(std::string(Variant::get_type_name(Variant::VECTOR3)) == "Vector3");
}

// ===========================================================================
// Object pointer
// ===========================================================================
TEST_CASE("Variant: nullptr becomes NIL")
{
    Variant v(static_cast<Object*>(nullptr));
    CHECK(v.get_type() == Variant::NIL);
}
