#include "services/graphics/frame_graph.h"
#include "services/graphics/transient_resource_pool.h"
#include "tests/mocks/mock_resource_graphics_device.h"

#include <doctest/doctest.h>

#include <limits>

using namespace rover;

namespace
{
    TextureDesc description()
    {
        TextureDesc d{};
        d.width = d.height = 16;
        d.mip_levels       = 3;
        d.array_layers     = 2;
        d.type             = TextureType::Texture2DArray;
        d.format           = Format::R8G8B8A8_UNORM;
        d.usage =
            TextureUsage::Storage | TextureUsage::Sampled | TextureUsage::ColorAttachment | TextureUsage::TransferSrc;
        return d;
    }

    TextureSubresourceRange slice(u32 mip, u32 layer = 0)
    {
        return {TextureAspect::Color, mip, 1, layer, 1};
    }
} // namespace

TEST_CASE("Texture ranges: remaining levels, aspect masks and overflow bounds normalize")
{
    TextureSubresourceRange result{};
    REQUIRE(normalize_texture_range(Format::R8G8B8A8_UNORM, 3, 2, {TextureAspect::Automatic, 1, 0, 0, 0}, result));
    CHECK(result == TextureSubresourceRange{TextureAspect::Color, 1, 2, 0, 2});
    CHECK_FALSE(normalize_texture_range(Format::R8G8B8A8_UNORM, 3, 2, {TextureAspect::Depth}, result));
    CHECK_FALSE(normalize_texture_range(Format::R8G8B8A8_UNORM, 3, 2, {TextureAspect::Color, 3}, result));
    CHECK_FALSE(normalize_texture_range(
        Format::R8G8B8A8_UNORM, 3, 2, {TextureAspect::Color, 1, std::numeric_limits<u32>::max()}, result));
    REQUIRE(normalize_texture_range(Format::D32_SFLOAT_S8_UINT, 1, 2, {}, result));
    CHECK(result.aspects == (TextureAspect::Depth | TextureAspect::Stencil));
    CHECK(texture_plane_bytes(Format::D32_SFLOAT_S8_UINT, TextureAspect::Depth) == 4);
    CHECK(texture_plane_bytes(Format::D32_SFLOAT_S8_UINT, TextureAspect::Stencil) == 1);
    CHECK(texture_plane_bytes(Format::D32_SFLOAT_S8_UINT, result.aspects) == 0);
    CHECK(texture_mip_extent(16, 2) == 4);
    CHECK(texture_mip_extent(16, 40) == 1);
}

TEST_CASE("FrameGraph subresources: different mips and layers have independent producers")
{
    FrameGraph graph;
    const auto image = graph.import_texture("array", 11, description());
    graph.add_pass("read all", [=](PassBuilder& b) { b.read(image); }, {});
    graph.add_pass(
        "write mip0 layer1", [=](PassBuilder& b) { b.write(image, ResourceAccess::StorageWrite, slice(0, 1)); }, {});
    graph.add_pass(
        "write mip1 layer0", [=](PassBuilder& b) { b.write(image, ResourceAccess::StorageWrite, slice(1, 0)); }, {});
    REQUIRE(graph.compile());
    CHECK(graph.dependencies()[0] == std::vector<u32>{1, 2});
    CHECK(graph.dependencies()[1].empty());
    CHECK(graph.dependencies()[2].empty());
    CHECK(graph.execution_order() == std::vector<u32>{1, 2, 0});
    CHECK(graph.barriers()[1][0].range == slice(0, 1));
    CHECK(graph.barriers()[2][0].range == slice(1, 0));
    MockGraphicsDevice device;
    REQUIRE(graph.execute(device));
    CHECK(device.barrier_batches[0].textures[0].range == slice(0, 1));
}

TEST_CASE(
    "FrameGraph subresources: partial overwrite preserves inherited values and only waits for overlapping readers")
{
    FrameGraph graph;
    const auto a = graph.import_texture("a", 1, description());
    const auto b = graph.create_version(a);
    const auto c = graph.create_version(b);
    graph.add_pass("read c", [=](PassBuilder& p) { p.read(c); }, {});
    graph.add_pass("overwrite c mip0", [=](PassBuilder& p) { p.write(c, ResourceAccess::StorageWrite, slice(0)); }, {});
    graph.add_pass(
        "read inherited b mip0", [=](PassBuilder& p) { p.read(b, ResourceAccess::ShaderRead, slice(0)); }, {});
    graph.add_pass("write b mip1", [=](PassBuilder& p) { p.write(b, ResourceAccess::StorageWrite, slice(1)); }, {});
    graph.add_pass("read original mip0", [=](PassBuilder& p) { p.read(a, ResourceAccess::ShaderRead, slice(0)); }, {});
    graph.add_pass("initialize", [=](PassBuilder& p) { p.write(a); }, {});
    REQUIRE(graph.compile());
    CHECK(graph.dependencies()[1] == std::vector<u32>{2, 4, 5});
    CHECK(graph.dependencies()[3] == std::vector<u32>{5});
    CHECK(graph.dependencies()[0] == std::vector<u32>{1, 3, 5});
    CHECK(graph.execution_order() == std::vector<u32>{5, 2, 3, 4, 1, 0});
}

TEST_CASE("FrameGraph subresources: nonoverlapping same-pass uses and depth/stencil aspects remain independent")
{
    FrameGraph graph;
    auto       desc  = description();
    const auto image = graph.import_texture("array", 1, desc);
    SUBCASE("different mips")
    {
        graph.add_pass("mixed",
                       [=](PassBuilder& p) {
                           p.read(image, ResourceAccess::ShaderRead, slice(0));
                           p.write(image, ResourceAccess::StorageWrite, slice(1));
                       },
                       {});
    }
    SUBCASE("depth and stencil")
    {
        desc.format      = Format::D32_SFLOAT_S8_UINT;
        const auto depth = graph.import_texture("depth", 2, desc);
        graph.add_pass("depth",
                       [=](PassBuilder& p) { p.write(depth, ResourceAccess::TransferWrite, {TextureAspect::Depth}); },
                       {});
        graph.add_pass("stencil",
                       [=](PassBuilder& p) { p.write(depth, ResourceAccess::TransferWrite, {TextureAspect::Stencil}); },
                       {});
        graph.add_pass("read depth",
                       [=](PassBuilder& p) { p.read(depth, ResourceAccess::TransferRead, {TextureAspect::Depth}); },
                       {});
    }
    REQUIRE(graph.compile());
    if (graph.pass_count() == 3)
    {
        CHECK(graph.dependencies()[2] == std::vector<u32>{0});
        CHECK(graph.dependencies()[1].empty());
    }
}

TEST_CASE("FrameGraph subresources: invalid and overlapping requests fail compilation atomically")
{
    FrameGraph graph;
    const auto image = graph.import_texture("array", 1, description());
    SUBCASE("overlapping writers")
    {
        graph.add_pass(
            "a",
            [=](PassBuilder& p) { p.write(image, ResourceAccess::StorageWrite, {TextureAspect::Color, 0, 2, 0, 1}); },
            {});
        graph.add_pass("b", [=](PassBuilder& p) { p.write(image, ResourceAccess::StorageWrite, slice(1)); }, {});
    }
    SUBCASE("overlapping feedback")
    {
        graph.add_pass("a",
                       [=](PassBuilder& p) {
                           p.read(image, ResourceAccess::ShaderRead, slice(0));
                           p.write(image);
                       },
                       {});
    }
    SUBCASE("invalid mip")
    {
        graph.add_pass("a", [=](PassBuilder& p) { p.read(image, ResourceAccess::ShaderRead, slice(3)); }, {});
    }
    SUBCASE("invalid layer")
    {
        graph.add_pass("a", [=](PassBuilder& p) { p.read(image, ResourceAccess::ShaderRead, slice(0, 2)); }, {});
    }
    SUBCASE("wrong aspect")
    {
        graph.add_pass(
            "a", [=](PassBuilder& p) { p.read(image, ResourceAccess::ShaderRead, {TextureAspect::Depth}); }, {});
    }
    SUBCASE("range on buffer")
    {
        const auto buffer = graph.import_buffer("buffer", 1);
        graph.add_pass("a", [=](PassBuilder& p) { p.read(buffer, ResourceAccess::UniformRead, slice(0)); }, {});
    }
    CHECK_FALSE(graph.compile());
    CHECK(graph.dependencies().empty());
    CHECK(graph.barriers().empty());
}

TEST_CASE("FrameGraph subresources: exports reject overwritten cells while preserving unmodified old cells")
{
    FrameGraph graph;
    const auto a = graph.import_texture("array", 1, description());
    const auto b = graph.create_version(a);
    graph.add_pass("overwrite mip1", [=](PassBuilder& p) { p.write(b, ResourceAccess::StorageWrite, slice(1)); }, {});
    graph.set_final_access(a, ResourceAccess::ShaderRead, slice(0));
    REQUIRE(graph.compile());
    REQUIRE(graph.final_barriers().size() == 1);
    CHECK(graph.final_barriers()[0].range == slice(0));
    graph.set_final_access(a, ResourceAccess::ShaderRead, slice(1));
    CHECK_FALSE(graph.compile());
    graph.set_final_access(a, ResourceAccess::Undefined, slice(1));
    graph.set_final_access(b, ResourceAccess::TransferRead, slice(0));
    CHECK_FALSE(graph.compile()); // Same physical cell requested in two layouts.
}

TEST_CASE("FrameGraph subresources: unwritten transient cells cannot be inherited as initialized data")
{
    FrameGraph graph;
    const auto a = graph.create_texture("array", description());
    graph.add_pass("write mip0", [=](PassBuilder& p) { p.write(a, ResourceAccess::StorageWrite, slice(0)); }, {});
    graph.add_pass("read all", [=](PassBuilder& p) { p.read(a); }, {});
    CHECK_FALSE(graph.compile());
}

TEST_CASE("FrameGraph subresources: pooled attachment and binding caches include view ranges")
{
    MockResourceGraphicsDevice device;
    TransientResourcePool      pool(device);
    FrameGraph                 graph;
    const auto                 image = graph.create_texture("array", description());
    const auto                 first = graph.add_pass("mip0", {}, {});
    graph.set_render_target(first, 900, image, {}, INVALID_RESOURCE_ID, slice(0));
    const auto second = graph.add_pass("mip1 layer1", {}, {});
    graph.set_render_target(second, 900, image, {}, INVALID_RESOURCE_ID, slice(1, 1));
    graph.add_pass("sample",
                   [=](PassBuilder& p) {
                       p.read(image, ResourceAccess::ShaderRead, slice(0));
                       p.read(image, ResourceAccess::ShaderRead, slice(1, 1));
                   },
                   {});
    RenderBinding binding{};
    binding.resource            = image;
    binding.entry.type          = BindingType::SampledTexture;
    binding.entry.texture_range = slice(0);
    const auto group0           = graph.create_bind_group(902, {binding});
    binding.entry.texture_range = slice(1, 1);
    const auto group1           = graph.create_bind_group(902, {binding});
    REQUIRE(graph.compile());
    REQUIRE(graph.prepare_resources(pool));
    CHECK(graph.passes()[0].framebuffer != graph.passes()[1].framebuffer);
    CHECK(graph.bind_group(group0) != graph.bind_group(group1));
    CHECK(pool.texture_count() == 1);
}
