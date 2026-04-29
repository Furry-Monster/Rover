// Unit tests for Sprint 2.6 mesh + texture data + procedural generators.
// The GPU upload paths are exercised end-to-end in the integration demo;
// here we cover only CPU-side correctness.

#include "modules/serialization/mesh_data.h"
#include "modules/serialization/primitive_meshes.h"
#include "modules/serialization/texture_data.h"

#include <doctest/doctest.h>

using namespace rover;

TEST_CASE("make_cube produces 24 vertices / 36 indices with unit normals")
{
    auto m = make_cube(2.0f);
    CHECK(m.vertex_count() == 24);
    CHECK(m.index_count() == 36);
    for (const auto& v : m.vertices)
    {
        const f32 n = v.normal.length();
        CHECK(n == doctest::Approx(1.0f));
    }
}

TEST_CASE("make_quad produces 4 vertices in the XY plane")
{
    auto m = make_quad(1.0f);
    CHECK(m.vertex_count() == 4);
    CHECK(m.index_count() == 6);
    for (const auto& v : m.vertices)
    {
        CHECK(v.position.z() == doctest::Approx(0.0f));
        CHECK(v.normal == Vector3{0.0f, 0.0f, 1.0f});
    }
}

TEST_CASE("make_sphere produces (lat+1)*(lon+1) verts and degenerates to identity radius")
{
    auto m = make_sphere(1.0f, 4, 6);
    CHECK(m.vertex_count() == (4u + 1u) * (6u + 1u));
    CHECK(m.index_count() == 4u * 6u * 6u);
    for (const auto& v : m.vertices)
    {
        const f32 r = v.position.length();
        CHECK(r == doctest::Approx(1.0f));
    }
}

TEST_CASE("make_solid_rgba fills uniformly")
{
    auto t = make_solid_rgba(4, 4, 200, 100, 50, 255);
    REQUIRE(t.byte_size() == 4u * 4u * 4u);
    for (usize i = 0; i < t.pixels.size(); i += 4)
    {
        CHECK(t.pixels[i + 0] == 200);
        CHECK(t.pixels[i + 1] == 100);
        CHECK(t.pixels[i + 2] == 50);
        CHECK(t.pixels[i + 3] == 255);
    }
}

TEST_CASE("make_checkerboard alternates cells")
{
    auto t = make_checkerboard(4, 4, 1, 0, 0, 0, 255, 255, 255);
    // First pixel (0,0) -> first color (cx+cy = 0 -> first=true)
    CHECK(t.pixels[0] == 0);
    // Pixel (1,0) -> second color
    CHECK(t.pixels[4] == 255);
}
