// Unit tests for core/math/.
//
// Exercise: vector arithmetic, matrix factories, quaternion rotation,
// AABB containment, Transform3D round-trip. Values are chosen so that
// trivially-wrong implementations (return zero, swap operands, drop a
// component) produce visibly wrong results.

#include "core/math/aabb.h"
#include "core/math/mat4.h"
#include "core/math/math_defs.h"
#include "core/math/quat.h"
#include "core/math/transform3d.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"

#include <doctest/doctest.h>

using namespace rover;

// ---------------------------------------------------------------------------
// math_defs
// ---------------------------------------------------------------------------
TEST_CASE("math_defs: PI and TAU")
{
    CHECK(PI == doctest::Approx(3.14159265358979323846));
    CHECK(TAU == doctest::Approx(2.0 * PI));
}

TEST_CASE("math_defs: lerp interpolates linearly")
{
    CHECK(lerp(0.0, 10.0, 0.0) == doctest::Approx(0.0));
    CHECK(lerp(0.0, 10.0, 0.5) == doctest::Approx(5.0));
    CHECK(lerp(0.0, 10.0, 1.0) == doctest::Approx(10.0));
    CHECK(lerp(-2.0, 2.0, 0.25) == doctest::Approx(-1.0));
}

TEST_CASE("math_defs: clamp respects bounds")
{
    CHECK(clamp(5, 0, 10) == 5);
    CHECK(clamp(-3, 0, 10) == 0);
    CHECK(clamp(99, 0, 10) == 10);
    CHECK(clamp(7.5, 1.0, 5.0) == doctest::Approx(5.0));
}

TEST_CASE("math_defs: is_nearly_equal / zero")
{
    CHECK(is_nearly_equal(1.0, 1.0 + 1e-9));
    CHECK_FALSE(is_nearly_equal(1.0, 1.1));
    CHECK(is_nearly_zero(1e-9));
    CHECK_FALSE(is_nearly_zero(0.5));
}

// ---------------------------------------------------------------------------
// Vector2
// ---------------------------------------------------------------------------
TEST_CASE("Vector2: construction and accessors")
{
    Vector2 v{3.0f, 4.0f};
    CHECK(v.x() == doctest::Approx(3.0f));
    CHECK(v.y() == doctest::Approx(4.0f));
    CHECK(Vector2{}.x() == doctest::Approx(0.0f));
}

TEST_CASE("Vector2: arithmetic")
{
    Vector2 a{1.0f, 2.0f}, b{4.0f, 6.0f};
    CHECK((a + b) == Vector2{5.0f, 8.0f});
    CHECK((b - a) == Vector2{3.0f, 4.0f});
    CHECK((a * 2.0f) == Vector2{2.0f, 4.0f});
    CHECK((2.0f * a) == Vector2{2.0f, 4.0f}); // commutative scalar
    CHECK((-a) == Vector2{-1.0f, -2.0f});
}

TEST_CASE("Vector2: length and normalization")
{
    Vector2 v{3.0f, 4.0f};
    CHECK(v.length_squared() == doctest::Approx(25.0f));
    CHECK(v.length() == doctest::Approx(5.0f));
    Vector2 n = v.normalized();
    CHECK(n.length() == doctest::Approx(1.0f));
    CHECK(n.x() == doctest::Approx(0.6f));
    CHECK(n.y() == doctest::Approx(0.8f));
}

TEST_CASE("Vector2: dot and 2D cross")
{
    // Distinct asymmetric values catch swapped-operand and dropped-component bugs.
    Vector2 a{1.0f, 2.0f}, b{4.0f, 5.0f};
    CHECK(a.dot(b) == doctest::Approx(1 * 4 + 2 * 5)); // 14
    // 2D "cross" returns the z component of the 3D cross product.
    CHECK(a.cross(b) == doctest::Approx(1 * 5 - 2 * 4)); // -3
    CHECK(b.cross(a) == doctest::Approx(-a.cross(b)));   // anti-commutative
}

TEST_CASE("Vector2: static directions")
{
    CHECK(Vector2::ZERO == Vector2{0.0f, 0.0f});
    CHECK(Vector2::UP == Vector2{0.0f, 1.0f});
    CHECK(Vector2::DOWN == Vector2{0.0f, -1.0f});
    CHECK(Vector2::RIGHT == Vector2{1.0f, 0.0f});
    CHECK(Vector2::LEFT == Vector2{-1.0f, 0.0f});
}

// ---------------------------------------------------------------------------
// Vector3
// ---------------------------------------------------------------------------
TEST_CASE("Vector3: dot product asymmetric values")
{
    Vector3 a{1.0f, 2.0f, 3.0f};
    Vector3 b{4.0f, 5.0f, 6.0f};
    CHECK(a.dot(b) == doctest::Approx(1 * 4 + 2 * 5 + 3 * 6)); // 32
}

TEST_CASE("Vector3: 3D cross product is perpendicular")
{
    Vector3 x{1.0f, 0.0f, 0.0f};
    Vector3 y{0.0f, 1.0f, 0.0f};
    CHECK(x.cross(y) == Vector3{0.0f, 0.0f, 1.0f});  // right-handed
    CHECK(y.cross(x) == Vector3{0.0f, 0.0f, -1.0f}); // anti-commutative
    Vector3 a{1.0f, 2.0f, 3.0f};
    CHECK(a.cross(a).length_squared() == doctest::Approx(0.0f));
}

TEST_CASE("Vector3: forward direction is -Z (right-handed convention)")
{
    CHECK(Vector3::FORWARD == Vector3{0.0f, 0.0f, -1.0f});
    CHECK(Vector3::BACK == Vector3{0.0f, 0.0f, 1.0f});
}

// ---------------------------------------------------------------------------
// Vector4
// ---------------------------------------------------------------------------
TEST_CASE("Vector4: dot includes w component")
{
    Vector4 a{1.0f, 2.0f, 3.0f, 4.0f};
    Vector4 b{5.0f, 6.0f, 7.0f, 8.0f};
    CHECK(a.dot(b) == doctest::Approx(1 * 5 + 2 * 6 + 3 * 7 + 4 * 8)); // 70
}

// ---------------------------------------------------------------------------
// Mat4
// ---------------------------------------------------------------------------
TEST_CASE("Mat4: identity is identity")
{
    Mat4    m = Mat4::identity();
    Vector4 v{2.0f, 3.0f, 5.0f, 1.0f};
    CHECK((m * v) == v);
}

TEST_CASE("Mat4: translate moves a homogeneous point")
{
    Mat4    t = Mat4::translate(Vector3{10.0f, 20.0f, 30.0f});
    Vector4 origin{0.0f, 0.0f, 0.0f, 1.0f};
    Vector4 moved = t * origin;
    CHECK(moved.x() == doctest::Approx(10.0f));
    CHECK(moved.y() == doctest::Approx(20.0f));
    CHECK(moved.z() == doctest::Approx(30.0f));
    CHECK(moved.w() == doctest::Approx(1.0f));
}

TEST_CASE("Mat4: scale scales components independently")
{
    Mat4    s = Mat4::scale(Vector3{2.0f, 3.0f, 4.0f});
    Vector4 v{1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 r = s * v;
    CHECK(r.x() == doctest::Approx(2.0f));
    CHECK(r.y() == doctest::Approx(3.0f));
    CHECK(r.z() == doctest::Approx(4.0f));
}

TEST_CASE("Mat4: inverse(translate) cancels the translation")
{
    Mat4    t = Mat4::translate(Vector3{7.0f, -3.0f, 5.0f});
    Mat4    i = t.inverse();
    Vector4 v{0.0f, 0.0f, 0.0f, 1.0f};
    Vector4 r = i * (t * v);
    CHECK(r.x() == doctest::Approx(0.0f));
    CHECK(r.y() == doctest::Approx(0.0f));
    CHECK(r.z() == doctest::Approx(0.0f));
}

// ---------------------------------------------------------------------------
// Quat
// ---------------------------------------------------------------------------
TEST_CASE("Quat: identity rotation leaves vectors unchanged")
{
    Quat    q = Quat::identity();
    Vector3 v{1.0f, 2.0f, 3.0f};
    Vector3 r = q * v;
    CHECK(r.x() == doctest::Approx(1.0f));
    CHECK(r.y() == doctest::Approx(2.0f));
    CHECK(r.z() == doctest::Approx(3.0f));
}

TEST_CASE("Quat: 90-degree rotation around Z maps +X -> +Y")
{
    Quat    q = Quat::from_axis_angle(Vector3{0.0f, 0.0f, 1.0f}, static_cast<f32>(PI * 0.5));
    Vector3 r = q * Vector3{1.0f, 0.0f, 0.0f};
    CHECK(r.x() == doctest::Approx(0.0f).epsilon(1e-5));
    CHECK(r.y() == doctest::Approx(1.0f).epsilon(1e-5));
    CHECK(r.z() == doctest::Approx(0.0f).epsilon(1e-5));
}

TEST_CASE("Quat: inverse cancels rotation")
{
    Quat    q   = Quat::from_axis_angle(Vector3{0.0f, 1.0f, 0.0f}, 0.7f);
    Quat    inv = q.inverse();
    Vector3 v{1.5f, 2.5f, 3.5f};
    Vector3 r = inv * (q * v);
    CHECK(r.x() == doctest::Approx(1.5f).epsilon(1e-5));
    CHECK(r.y() == doctest::Approx(2.5f).epsilon(1e-5));
    CHECK(r.z() == doctest::Approx(3.5f).epsilon(1e-5));
}

TEST_CASE("Quat: slerp endpoints are exact")
{
    Quat    a = Quat::identity();
    Quat    b = Quat::from_axis_angle(Vector3{1.0f, 0.0f, 0.0f}, 1.234f);
    Vector3 v{0.0f, 1.0f, 0.0f};
    Vector3 r0 = a.slerp(b, 0.0f) * v;
    Vector3 r1 = a.slerp(b, 1.0f) * v;
    Vector3 vb = b * v;
    CHECK(r0.x() == doctest::Approx(v.x()).epsilon(1e-5));
    CHECK(r1.x() == doctest::Approx(vb.x()).epsilon(1e-5));
    CHECK(r1.y() == doctest::Approx(vb.y()).epsilon(1e-5));
    CHECK(r1.z() == doctest::Approx(vb.z()).epsilon(1e-5));
}

// ---------------------------------------------------------------------------
// AABB
// ---------------------------------------------------------------------------
TEST_CASE("AABB: contains a point inside, rejects outside")
{
    AABB box{Vector3{-1.0f, -1.0f, -1.0f}, Vector3{1.0f, 1.0f, 1.0f}};
    CHECK(box.contains(Vector3::ZERO));
    CHECK(box.contains(Vector3{1.0f, 1.0f, 1.0f})); // boundary
    CHECK_FALSE(box.contains(Vector3{2.0f, 0.0f, 0.0f}));
    CHECK_FALSE(box.contains(Vector3{0.0f, 0.0f, -1.0001f}));
}

TEST_CASE("AABB: intersects detects overlap")
{
    AABB a{Vector3{0.0f, 0.0f, 0.0f}, Vector3{2.0f, 2.0f, 2.0f}};
    AABB b{Vector3{1.0f, 1.0f, 1.0f}, Vector3{3.0f, 3.0f, 3.0f}};
    AABB c{Vector3{5.0f, 5.0f, 5.0f}, Vector3{7.0f, 7.0f, 7.0f}};
    CHECK(a.intersects(b));
    CHECK_FALSE(a.intersects(c));
}

TEST_CASE("AABB: merge widens to enclose both")
{
    AABB a{Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f}};
    AABB b{Vector3{2.0f, 3.0f, 4.0f}, Vector3{5.0f, 6.0f, 7.0f}};
    AABB m = a.merge(b);
    CHECK(m.min == Vector3{0.0f, 0.0f, 0.0f});
    CHECK(m.max == Vector3{5.0f, 6.0f, 7.0f});
}

TEST_CASE("AABB: get_center and get_size")
{
    AABB box{Vector3{-2.0f, 0.0f, 4.0f}, Vector3{2.0f, 6.0f, 10.0f}};
    CHECK(box.get_center() == Vector3{0.0f, 3.0f, 7.0f});
    CHECK(box.get_size() == Vector3{4.0f, 6.0f, 6.0f});
}

// ---------------------------------------------------------------------------
// Transform3D
// ---------------------------------------------------------------------------
TEST_CASE("Transform3D: identity transforms points to themselves")
{
    Transform3D t = Transform3D::identity();
    Vector3     p{1.5f, 2.5f, 3.5f};
    CHECK(t.transform_point(p) == p);
}

TEST_CASE("Transform3D: pure translation moves origin to origin field")
{
    Transform3D t{Vector3{10.0f, 20.0f, 30.0f}, Quat::identity(), Vector3{1.0f, 1.0f, 1.0f}};
    Vector3     r = t.transform_point(Vector3::ZERO);
    CHECK(r.x() == doctest::Approx(10.0f));
    CHECK(r.y() == doctest::Approx(20.0f));
    CHECK(r.z() == doctest::Approx(30.0f));
}

TEST_CASE("Transform3D: scale composes with translation")
{
    Transform3D t{Vector3{1.0f, 2.0f, 3.0f}, Quat::identity(), Vector3{2.0f, 3.0f, 4.0f}};
    // Point (1,1,1) is scaled to (2,3,4) then translated to (3,5,7).
    Vector3 r = t.transform_point(Vector3{1.0f, 1.0f, 1.0f});
    CHECK(r.x() == doctest::Approx(3.0f));
    CHECK(r.y() == doctest::Approx(5.0f));
    CHECK(r.z() == doctest::Approx(7.0f));
}

TEST_CASE("Transform3D: transform_direction ignores translation")
{
    Transform3D t{Vector3{100.0f, 200.0f, 300.0f}, Quat::identity(), Vector3{1.0f, 1.0f, 1.0f}};
    Vector3     dir{1.0f, 0.0f, 0.0f};
    Vector3     r = t.transform_direction(dir);
    CHECK(r.x() == doctest::Approx(1.0f));
    CHECK(r.y() == doctest::Approx(0.0f));
    CHECK(r.z() == doctest::Approx(0.0f));
}

TEST_CASE("Transform3D: inverse round-trip")
{
    Transform3D t{
        Vector3{5.0f, -3.0f, 7.0f}, Quat::from_axis_angle(Vector3{0.0f, 1.0f, 0.0f}, 0.4f), Vector3{2.0f, 2.0f, 2.0f}};
    Transform3D inv = t.inverse();
    Vector3     p{1.0f, 2.0f, 3.0f};
    Vector3     r = inv.transform_point(t.transform_point(p));
    CHECK(r.x() == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(r.y() == doctest::Approx(2.0f).epsilon(1e-4));
    CHECK(r.z() == doctest::Approx(3.0f).epsilon(1e-4));
}
