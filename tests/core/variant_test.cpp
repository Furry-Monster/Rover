// Unit tests for core/variant/.

#include "core/variant/callable.h"
#include "core/variant/variant.h"

#include <doctest/doctest.h>

using namespace rover;

TEST_CASE("Variant: default-constructed is Nil")
{
    Variant v;
    CHECK(v.is_nil());
    CHECK(v.type() == Variant::Type::Nil);
}

TEST_CASE("Variant: scalar round-trip")
{
    Variant b{true};
    CHECK(b.type() == Variant::Type::Bool);
    CHECK(b.as_bool() == true);

    Variant i{i64{42}};
    CHECK(i.type() == Variant::Type::Int);
    CHECK(i.as_int() == 42);

    Variant f{3.5};
    CHECK(f.type() == Variant::Type::Float);
    CHECK(f.as_float() == doctest::Approx(3.5));
}

TEST_CASE("Variant: numeric coercion in accessors")
{
    Variant i{i64{5}};
    CHECK(i.as_float() == doctest::Approx(5.0));

    Variant f{2.5};
    CHECK(f.as_int() == 2);
    CHECK(f.as_bool() == true);
}

TEST_CASE("Variant: string is heap-shared and equal")
{
    Variant a{std::string{"hello"}};
    Variant b{a};
    CHECK(a == b);
    CHECK(a.as_string() == "hello");
    CHECK(b.as_string() == "hello");
}

TEST_CASE("Variant: math types round-trip")
{
    Variant v3{Vector3{1, 2, 3}};
    CHECK(v3.type() == Variant::Type::Vector3);
    CHECK(v3.as_vector3() == Vector3{1, 2, 3});

    Variant q{Quat::from_axis_angle(Vector3::UP, 1.0f)};
    CHECK(q.type() == Variant::Type::Quat);

    Variant m{Mat4::identity()};
    CHECK(m.type() == Variant::Type::Mat4);
    CHECK(m.as_mat4() == Mat4::identity());
}

TEST_CASE("Variant: array stores nested variants")
{
    VariantArray arr;
    arr.emplace_back(i64{1});
    arr.emplace_back(std::string{"two"});
    arr.emplace_back(Vector2{3, 4});
    Variant v{std::move(arr)};

    CHECK(v.type() == Variant::Type::Array);
    REQUIRE(v.as_array().size() == 3);
    CHECK(v.as_array()[0].as_int() == 1);
    CHECK(v.as_array()[1].as_string() == "two");
    CHECK(v.as_array()[2].as_vector2() == Vector2{3, 4});
}

TEST_CASE("Variant: dictionary stores keyed variants")
{
    VariantDict d;
    d["name"]  = Variant{std::string{"hero"}};
    d["level"] = Variant{i64{7}};
    Variant v{std::move(d)};

    CHECK(v.type() == Variant::Type::Dictionary);
    REQUIRE(v.as_dict().size() == 2);
    CHECK(v.as_dict().at("name").as_string() == "hero");
    CHECK(v.as_dict().at("level").as_int() == 7);
}

TEST_CASE("Variant: assignment frees previous heap data")
{
    Variant v{std::string{"old"}};
    v = Variant{i64{42}};
    CHECK(v.type() == Variant::Type::Int);
    CHECK(v.as_int() == 42);
}

TEST_CASE("Variant: move leaves source as Nil")
{
    Variant a{std::string{"data"}};
    Variant b{std::move(a)};
    CHECK(b.as_string() == "data");
    CHECK(a.is_nil());
}

TEST_CASE("Variant: type_name reports the current type")
{
    CHECK(std::string{Variant{}.type_name()} == "Nil");
    CHECK(std::string{Variant{true}.type_name()} == "Bool");
    CHECK(std::string{Variant{i64{1}}.type_name()} == "Int");
    CHECK(std::string{Variant{2.0}.type_name()} == "Float");
    CHECK(std::string{Variant{std::string{"s"}}.type_name()} == "String");
    CHECK(std::string{Variant{Vector3{}}.type_name()} == "Vector3");
}

// ---------------------------------------------------------------------------
// Callable
// ---------------------------------------------------------------------------
TEST_CASE("Callable: lambda invocation returns a Variant")
{
    Callable c = Callable::from_lambda([](const VariantArray& args) -> Variant {
        i64 sum = 0;
        for (const auto& a : args)
        {
            sum += a.as_int();
        }
        return Variant{sum};
    });
    CHECK(c.is_valid());

    VariantArray args;
    args.emplace_back(i64{1});
    args.emplace_back(i64{2});
    args.emplace_back(i64{3});
    Variant out = c(args);
    CHECK(out.as_int() == 6);
}

TEST_CASE("Callable: invalid callable returns Nil")
{
    Callable c;
    CHECK_FALSE(c.is_valid());
    Variant out = c(VariantArray{});
    CHECK(out.is_nil());
}
