#include "services/graphics/frame_graph.h"
#include "services/graphics/transient_resource_pool.h"
#include "tests/mocks/mock_resource_graphics_device.h"

#include <doctest/doctest.h>

using namespace rover;

namespace
{
    TextureDesc texture_desc(u32 width = 16)
    {
        TextureDesc desc{};
        desc.width  = width;
        desc.height = 16;
        desc.format = Format::R8G8B8A8_UNORM;
        desc.usage  = TextureUsage::Storage | TextureUsage::Sampled | TextureUsage::ColorAttachment;
        return desc;
    }

    RenderResourceId write_texture(FrameGraph& graph, u32 width = 16)
    {
        const auto id = graph.create_texture("texture", texture_desc(width));
        graph.add_pass("write", [=](PassBuilder& b) { b.write(id); }, {});
        return id;
    }

    void bound_graph(FrameGraph& graph, u32 width = 16)
    {
        const auto texture = graph.create_texture("texture", texture_desc(width));
        const auto buffer =
            graph.create_buffer("readback", {width * 16u * 4u, BufferUsage::Transfer, MemoryUsage::GpuToCpu});
        const auto write = graph.add_pass("render", {}, {});
        graph.set_render_target(write, 900, texture, {});
        graph.add_pass("read",
                       [=](PassBuilder& b) {
                           b.read(texture);
                           b.write(buffer, ResourceAccess::TransferWrite);
                       },
                       {});
        graph.set_final_access(buffer, ResourceAccess::HostRead);
        RenderBinding image{}, sampler{};
        image.entry.type      = BindingType::SampledTexture;
        image.resource        = texture;
        sampler.entry.binding = 1;
        sampler.entry.type    = BindingType::Sampler;
        sampler.entry.sampler = 901;
        graph.create_bind_group(902, {image, sampler});
    }
} // namespace

TEST_CASE("Transient pool: overlapping submissions get distinct resources and completed resources are reused")
{
    MockResourceGraphicsDevice device;
    TransientResourcePool      pool(device);
    FrameGraph                 graph;
    const auto                 first = write_texture(graph);
    REQUIRE(graph.compile());
    CHECK_FALSE(graph.execute(device));
    REQUIRE(graph.prepare_resources(pool));
    const auto a = graph.resource(first).texture;
    REQUIRE(a != INVALID_HANDLE);
    const auto next = device.next;
    REQUIRE(graph.prepare_resources(pool));
    CHECK(device.next == next);
    REQUIRE(graph.execute(device));
    device.serial = 2;
    CHECK_FALSE(graph.execute(device));
    graph.reset();
    const auto second = write_texture(graph);
    REQUIRE(graph.compile());
    REQUIRE(graph.prepare_resources(pool));
    CHECK(graph.resource(second).texture != a);
    device.serial    = 3;
    device.completed = 1;
    graph.reset();
    const auto third = write_texture(graph);
    REQUIRE(graph.compile());
    REQUIRE(graph.prepare_resources(pool));
    CHECK(graph.resource(third).texture == a);
    CHECK(pool.texture_count() == 2);
    CHECK(device.idle_count == 0);
}

TEST_CASE("Transient pool: lifetime intervals reuse GPU objects and keep exported or host buffers separate")
{
    MockResourceGraphicsDevice device;
    TransientResourcePool      pool(device);
    FrameGraph                 graph;
    const auto                 a = graph.create_texture("a", texture_desc());
    const auto                 b = graph.create_texture("b", texture_desc());
    graph.add_pass("write a", [=](PassBuilder& p) { p.write(a); }, {});
    graph.add_pass("read a", [=](PassBuilder& p) { p.read(a); }, {});
    graph.add_pass("write b", [=](PassBuilder& p) { p.write(b); }, {});
    graph.add_pass("read b", [=](PassBuilder& p) { p.read(b); }, {});
    bool reuse = true;
    SUBCASE("disjoint texture lifetimes") {}
    SUBCASE("export pins the first texture through graph end")
    {
        graph.set_final_access(a, ResourceAccess::ShaderRead);
        reuse = false;
    }
    SUBCASE("overlapping texture lifetimes")
    {
        graph.add_pass("late a reader", [=](PassBuilder& p) { p.read(a); }, {});
        reuse = false;
    }
    REQUIRE(graph.compile());
    CHECK(graph.resource(a).first_use == 0);
    REQUIRE(graph.prepare_resources(pool));
    CHECK((graph.resource(a).texture == graph.resource(b).texture) == reuse);
}

TEST_CASE("Transient pool: CPU-visible buffers cannot alias within a submission")
{
    MockResourceGraphicsDevice device;
    TransientResourcePool      pool(device);
    FrameGraph                 graph;
    MemoryUsage                memory = MemoryUsage::CpuToGpu;
    bool                       reuse  = false;
    SUBCASE("host writes before submit") {}
    SUBCASE("host reads after submit")
    {
        memory = MemoryUsage::GpuToCpu;
    }
    SUBCASE("GPU-only buffers may reuse nonoverlapping lifetimes")
    {
        memory = MemoryUsage::GpuOnly;
        reuse  = true;
    }
    const BufferDesc desc{64, BufferUsage::Storage, memory};
    const auto       a = graph.create_buffer("a", desc);
    const auto       b = graph.create_buffer("b", desc);
    graph.add_pass("write a", [=](PassBuilder& p) { p.write(a); }, {});
    graph.add_pass("read a", [=](PassBuilder& p) { p.read(a); }, {});
    graph.add_pass("write b", [=](PassBuilder& p) { p.write(b); }, {});
    graph.add_pass("read b", [=](PassBuilder& p) { p.read(b); }, {});
    REQUIRE(graph.compile());
    REQUIRE(graph.prepare_resources(pool));
    CHECK((graph.resource(a).buffer == graph.resource(b).buffer) == reuse);
}

TEST_CASE("Transient pool: resource versions retain one allocation and their whole lifetime")
{
    MockResourceGraphicsDevice device;
    TransientResourcePool      pool(device);
    FrameGraph                 graph;
    const auto                 a       = write_texture(graph);
    const auto                 version = graph.create_version(a);
    graph.add_pass("read old", [=](PassBuilder& p) { p.read(a); }, {});
    graph.add_pass("overwrite", [=](PassBuilder& p) { p.write(version); }, {});
    graph.add_pass("read new", [=](PassBuilder& p) { p.read(version); }, {});
    REQUIRE(graph.compile());
    CHECK(graph.resource(a).first_use == 0);
    CHECK(graph.resource(a).last_use == 3);
    CHECK(graph.resource(version).last_use == 3);
    REQUIRE(graph.prepare_resources(pool));
    CHECK(graph.resource(a).texture == graph.resource(version).texture);
    CHECK(pool.texture_count() == 1);
}

TEST_CASE("Transient pool: failure at each resource or dependent allocation rolls back")
{
    for (u64 offset = 0; offset < 4; ++offset)
    {
        MockResourceGraphicsDevice device;
        TransientResourcePool      pool(device);
        FrameGraph                 old_graph;
        bound_graph(old_graph);
        REQUIRE(old_graph.compile());
        REQUIRE(old_graph.prepare_resources(pool));
        const auto before = device.live;
        FrameGraph replacement;
        bound_graph(replacement, 32);
        REQUIRE(replacement.compile());
        device.serial    = 2;
        device.completed = 1;
        device.fail_at   = device.next + offset;
        CHECK_FALSE(replacement.prepare_resources(pool));
        CHECK(device.live == before);
        CHECK(pool.texture_count() == 1);
        CHECK(pool.buffer_count() == 1);
        CHECK(replacement.resource(0).texture == INVALID_HANDLE);
        device.fail_at = 0;
        REQUIRE(replacement.prepare_resources(pool));
        REQUIRE(replacement.execute(device));
        device.serial    = 3;
        device.completed = 2;
        REQUIRE(pool.clear());
        CHECK(device.live.empty());
        CHECK_FALSE(replacement.execute(device)); // Pool epoch changed.
    }
}

TEST_CASE("Transient pool: bind groups and framebuffers cache immutable references")
{
    MockResourceGraphicsDevice device;
    TransientResourcePool      pool(device);
    FrameGraph                 graph;
    bound_graph(graph);
    REQUIRE(graph.compile());
    REQUIRE(graph.prepare_resources(pool));
    const auto live  = device.live;
    device.serial    = 2;
    device.completed = 1;
    graph.reset();
    bound_graph(graph);
    REQUIRE(graph.compile());
    REQUIRE(graph.prepare_resources(pool));
    CHECK(device.live == live);
    CHECK(graph.bind_group(0) != INVALID_HANDLE);
    CHECK(graph.bind_group(INVALID_BIND_GROUP_ID) == INVALID_HANDLE);
    pool.clear();
    CHECK(device.live.empty());
    REQUIRE(graph.prepare_resources(pool)); // Clearing invalidates the old resolution.
    CHECK_FALSE(device.live.empty());
}

TEST_CASE("Transient pool: descriptors and initialization are checked before allocation")
{
    MockResourceGraphicsDevice device;
    TransientResourcePool      pool(device);
    FrameGraph                 graph;
    SUBCASE("zero extent")
    {
        write_texture(graph, 0);
    }
    SUBCASE("uninitialized texture read")
    {
        const auto image = graph.create_texture("a", texture_desc());
        graph.add_pass("read", [=](PassBuilder& p) { p.read(image); }, {});
    }
    SUBCASE("uninitialized GPU buffer read")
    {
        const auto buffer = graph.create_buffer("a", {64, BufferUsage::Storage, MemoryUsage::GpuOnly});
        graph.add_pass("read", [=](PassBuilder& p) { p.read(buffer); }, {});
    }
    SUBCASE("uninitialized export")
    {
        const auto buffer = graph.create_buffer("a", {64, BufferUsage::Storage, MemoryUsage::GpuToCpu});
        graph.set_final_access(buffer, ResourceAccess::HostRead);
    }
    SUBCASE("wrong texture usage")
    {
        auto desc        = texture_desc();
        desc.usage       = TextureUsage::Sampled;
        const auto image = graph.create_texture("a", desc);
        graph.add_pass("write", [=](PassBuilder& p) { p.write(image); }, {});
    }
    SUBCASE("wrong binding resource kind")
    {
        const auto    image = write_texture(graph);
        RenderBinding binding{};
        binding.resource = image; // Default binding kind is UniformBuffer.
        graph.create_bind_group(1, {binding});
    }
    SUBCASE("depth dimensions mismatch")
    {
        const auto image = graph.create_texture("a", texture_desc());
        auto       desc  = texture_desc(32);
        desc.format      = Format::D32_SFLOAT;
        desc.usage       = TextureUsage::DepthStencilAttachment;
        const auto depth = graph.create_texture("depth", desc);
        const auto pass  = graph.add_pass("render", {}, {});
        graph.set_render_target(pass, 1, image, {}, depth);
    }
    CHECK_FALSE(graph.compile());
    CHECK_FALSE(graph.prepare_resources(pool));
    CHECK(device.live.empty());
}

TEST_CASE("Transient pool: host initialization and unused declarations allocate only needed resources")
{
    MockResourceGraphicsDevice device;
    TransientResourcePool      pool(device);
    FrameGraph                 graph;
    graph.create_texture("unused", texture_desc());
    const auto camera = graph.create_buffer("camera", {64, BufferUsage::Uniform, MemoryUsage::CpuToGpu});
    graph.add_pass("read camera", [=](PassBuilder& p) { p.read(camera, ResourceAccess::UniformRead); }, {});
    REQUIRE(graph.compile());
    device.serial = 0;
    CHECK_FALSE(graph.prepare_resources(pool));
    device.serial = 1;
    REQUIRE(graph.prepare_resources(pool));
    CHECK(pool.texture_count() == 0);
    CHECK(pool.buffer_count() == 1);
    CHECK(graph.resource(camera).buffer != INVALID_HANDLE);
}

TEST_CASE("Transient pool: clearing refuses to invalidate recorded but unsubmitted commands")
{
    MockResourceGraphicsDevice device;
    TransientResourcePool      pool(device);
    FrameGraph                 graph;
    write_texture(graph);
    REQUIRE(graph.compile());
    REQUIRE(graph.prepare_resources(pool));
    REQUIRE(graph.execute(device));
    const auto live = device.live;
    CHECK_FALSE(pool.clear());
    CHECK(device.live == live);
    device.serial    = 2;
    device.completed = 1;
    CHECK(pool.clear());
    CHECK(device.live.empty());
}
