#include "core/math/math.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// ===========================================================================
// Vector2
// ===========================================================================
TEST_CASE("Vector2: default is zero")
{
    Vector2 v;
    CHECK(v.x == 0);
    CHECK(v.y == 0);
}

TEST_CASE("Vector2: arithmetic")
{
    Vector2 a(1, 2);
    Vector2 b(3, 4);

    CHECK((a + b) == Vector2(4, 6));
    CHECK((a - b) == Vector2(-2, -2));
    CHECK((a * 2) == Vector2(2, 4));
    CHECK((-a) == Vector2(-1, -2));
}

TEST_CASE("Vector2: dot and cross")
{
    Vector2 a(1, 0);
    Vector2 b(0, 1);

    CHECK(a.dot(b) == doctest::Approx(0));
    CHECK(a.cross(b) == doctest::Approx(1));
}

TEST_CASE("Vector2: length and normalize")
{
    Vector2 v(3, 4);
    CHECK(v.length() == doctest::Approx(5));

    Vector2 n = v.normalized();
    CHECK(n.length() == doctest::Approx(1));
}

// ===========================================================================
// Vector3
// ===========================================================================
TEST_CASE("Vector3: default is zero")
{
    Vector3 v;
    CHECK(v.x == 0);
    CHECK(v.y == 0);
    CHECK(v.z == 0);
}

TEST_CASE("Vector3: cross product")
{
    Vector3 x(1, 0, 0);
    Vector3 y(0, 1, 0);
    Vector3 z = x.cross(y);

    CHECK(z.x == doctest::Approx(0));
    CHECK(z.y == doctest::Approx(0));
    CHECK(z.z == doctest::Approx(1));
}

TEST_CASE("Vector3: length and normalize")
{
    Vector3 v(1, 2, 2);
    CHECK(v.length() == doctest::Approx(3));

    Vector3 n = v.normalized();
    CHECK(n.length() == doctest::Approx(1));
}

TEST_CASE("Vector3: dot product")
{
    Vector3 a(1, 0, 0);
    Vector3 b(0, 1, 0);
    CHECK(a.dot(b) == doctest::Approx(0));

    CHECK(a.dot(a) == doctest::Approx(1));
}

// ===========================================================================
// Vector4
// ===========================================================================
TEST_CASE("Vector4: arithmetic and length")
{
    Vector4 v(1, 2, 3, 4);
    CHECK(v.length_squared() == doctest::Approx(30));

    Vector4 n = v.normalized();
    CHECK(n.length() == doctest::Approx(1));
}

// ===========================================================================
// Color
// ===========================================================================
TEST_CASE("Color: defaults")
{
    Color c;
    CHECK(c.r == 0);
    CHECK(c.g == 0);
    CHECK(c.b == 0);
    CHECK(c.a == 1);
}

TEST_CASE("Color: to/from rgba32")
{
    Color c(1, 0, 0, 1);
    uint32_t packed = c.to_rgba32();
    Color    unpacked = Color::from_rgba32(packed);

    CHECK(unpacked.r == doctest::Approx(1).epsilon(0.01));
    CHECK(unpacked.g == doctest::Approx(0));
    CHECK(unpacked.b == doctest::Approx(0));
    CHECK(unpacked.a == doctest::Approx(1).epsilon(0.01));
}

TEST_CASE("Color: luminance")
{
    Color white(1, 1, 1, 1);
    CHECK(white.luminance() == doctest::Approx(1));

    Color black(0, 0, 0, 1);
    CHECK(black.luminance() == doctest::Approx(0));
}

// ===========================================================================
// Quaternion
// ===========================================================================
TEST_CASE("Quaternion: identity")
{
    Quaternion q;
    CHECK(q.w == doctest::Approx(1));
    CHECK(q.length() == doctest::Approx(1));
}

TEST_CASE("Quaternion: axis-angle rotation")
{
    Quaternion q(Vector3::UP(), MATH_HALF_PI);
    Vector3    v(1, 0, 0);
    Vector3    rotated = q * v;

    CHECK(rotated.x == doctest::Approx(0).epsilon(0.001));
    CHECK(rotated.y == doctest::Approx(0).epsilon(0.001));
    CHECK(rotated.z == doctest::Approx(-1).epsilon(0.001));
}

TEST_CASE("Quaternion: inverse")
{
    Quaternion q(Vector3::UP(), 0.5f);
    Quaternion inv = q.inverse();
    Quaternion product = q * inv;

    CHECK(product.is_equal_approx(Quaternion::IDENTITY()));
}

// ===========================================================================
// Basis
// ===========================================================================
TEST_CASE("Basis: identity transforms")
{
    Basis   id;
    Vector3 v(1, 2, 3);

    CHECK((id * v).is_equal_approx(v));
}

TEST_CASE("Basis: transpose of identity is identity")
{
    Basis id;
    CHECK(id.transposed() == id);
}

TEST_CASE("Basis: determinant of identity is 1")
{
    Basis id;
    CHECK(id.determinant() == doctest::Approx(1));
}

TEST_CASE("Basis: quaternion round-trip")
{
    Quaternion q(Vector3::UP(), 0.7f);
    Basis      b(q);
    Quaternion q2 = b.get_quaternion();

    CHECK(q.is_equal_approx(q2));
}

// ===========================================================================
// Transform3D
// ===========================================================================
TEST_CASE("Transform3D: xform applies translation")
{
    Transform3D t(Basis(), Vector3(1, 2, 3));
    Vector3     v(0, 0, 0);

    CHECK(t.xform(v).is_equal_approx(Vector3(1, 2, 3)));
}

TEST_CASE("Transform3D: inverse round-trip")
{
    Quaternion  q(Vector3(0, 0, 1).normalized(), 0.3f);
    Transform3D t(q, Vector3(5, -3, 2));
    Transform3D inv = t.inverse();

    Vector3 v(1, 2, 3);
    Vector3 round_trip = inv.xform(t.xform(v));

    CHECK(round_trip.is_equal_approx(v));
}

// ===========================================================================
// AABB
// ===========================================================================
TEST_CASE("AABB: contains point")
{
    AABB box(Vector3(0, 0, 0), Vector3(1, 1, 1));

    CHECK(box.has_point(Vector3(0.5f, 0.5f, 0.5f)));
    CHECK_FALSE(box.has_point(Vector3(2, 0, 0)));
}

TEST_CASE("AABB: intersection")
{
    AABB a(Vector3(0, 0, 0), Vector3(2, 2, 2));
    AABB b(Vector3(1, 1, 1), Vector3(2, 2, 2));
    AABB c(Vector3(5, 5, 5), Vector3(1, 1, 1));

    CHECK(a.intersects(b));
    CHECK_FALSE(a.intersects(c));
}

// ===========================================================================
// Plane
// ===========================================================================
TEST_CASE("Plane: distance to point")
{
    Plane p(Vector3::UP(), 0);

    CHECK(p.distance_to(Vector3(0, 5, 0)) == doctest::Approx(5));
    CHECK(p.distance_to(Vector3(0, -3, 0)) == doctest::Approx(-3));
}

// ===========================================================================
// Matrix4
// ===========================================================================
TEST_CASE("Matrix4: identity multiply is identity")
{
    Matrix4 id;
    Matrix4 result = id * id;

    CHECK(result == Matrix4::IDENTITY());
}

TEST_CASE("Matrix4: perspective produces non-identity")
{
    Matrix4 p = Matrix4::perspective(math_deg_to_rad(60), 16.0f / 9.0f, 0.1f, 100.0f);
    CHECK(p != Matrix4::IDENTITY());
}
