#include "services/graphics/post_process_pipeline.h"
#include "tests/mocks/mock_resource_graphics_device.h"

#include <doctest/doctest.h>

#include <limits>
#include <unordered_map>
#include <unordered_set>

using namespace rover;

namespace
{
    using PostProcessDevice = MockResourceGraphicsDevice;

    bool initialize(PostProcessPipeline& pipeline, bool readback = false)
    {
        ShaderDesc vertex{}, fragment{};
        vertex.stage   = ShaderStage::Vertex;
        fragment.stage = ShaderStage::Fragment;
        return pipeline.initialize(vertex, fragment, fragment, readback);
    }
} // namespace

TEST_CASE("PostProcessPipeline: per-image targets survive steady frames and rebuild on image or extent changes")
{
    PostProcessDevice device;
    {
        PostProcessPipeline pipeline(device);
        REQUIRE(initialize(pipeline));
        REQUIRE(pipeline.prepare());
        CHECK(pipeline.generation() == 1);
        FrameGraph graph;
        auto       prepare_graph = [&]() {
            graph.reset();
            REQUIRE(pipeline.add_passes(graph, device.screens[0], {}, {}));
            REQUIRE(graph.compile());
            REQUIRE(pipeline.prepare_resources(graph));
        };
        prepare_graph();
        CHECK(device.textures.size() == 3);
        REQUIRE(device.layout_entries.size() == 2);
        CHECK(device.layout_entries[0].type == BindingType::SampledTexture);
        CHECK(device.layout_entries[1].type == BindingType::Sampler);
        const auto next = device.next;
        const auto idle = device.idle_count;
        REQUIRE(pipeline.prepare());
        CHECK(device.next == next);
        CHECK(device.idle_count == idle);
        const auto old = device.textures;
        device.width   = 48;
        device.height  = 36;
        REQUIRE(pipeline.prepare());
        CHECK(pipeline.generation() == 2);
        CHECK(pipeline.width() == 48);
        CHECK(pipeline.height() == 36);
        prepare_graph();
        for (const auto& [handle, desc] : old)
        {
            CHECK_FALSE(device.live.contains(handle));
        }
        for (const auto& [handle, desc] : device.textures)
        {
            CHECK(desc.width == 48);
            CHECK(desc.height == 36);
        }
        device.screens = {2001, 2002, 2003};
        device.format  = Format::R8G8B8A8_SRGB;
        REQUIRE(pipeline.prepare());
        CHECK(pipeline.generation() == 3);
        prepare_graph();
        CHECK(device.textures.size() == 3);
    }
    CHECK(device.live.empty());
}

TEST_CASE("PostProcessPipeline: allocation failure rolls back every partial target")
{
    // Static configuration replacement fails independently of pool allocation.
    for (u64 offset = 0; offset < 2; ++offset)
    {
        PostProcessDevice   device;
        PostProcessPipeline pipeline(device);
        REQUIRE(initialize(pipeline, true));
        REQUIRE(pipeline.prepare());
        const auto old = device.live;
        device.width   = 64;
        device.fail_at = device.next + offset;
        CHECK_FALSE(pipeline.prepare());
        CHECK(pipeline.generation() == 1);
        CHECK(pipeline.width() == 32);
        CHECK(device.live == old);
        device.fail_at = 0;
        REQUIRE(pipeline.prepare());
        CHECK(pipeline.generation() == 2);
        pipeline.shutdown();
        CHECK(device.live.empty());
    }
}

TEST_CASE("PostProcessPipeline: initialization failures release all resources")
{
    for (u64 failure = 1; failure <= 9; ++failure)
    {
        PostProcessDevice device;
        device.fail_at = failure;
        PostProcessPipeline pipeline(device);
        CHECK_FALSE(initialize(pipeline));
        CHECK(device.live.empty());
    }
}

TEST_CASE("PostProcessPipeline: graph orders sampling after HDR writes and exports readback for host access")
{
    PostProcessDevice   device;
    PostProcessPipeline pipeline(device);
    REQUIRE(initialize(pipeline, true));
    REQUIRE(pipeline.prepare());
    FrameGraph      graph;
    std::vector<u8> pixels;
    CHECK_FALSE(pipeline.read_pixels(device.screens[0], pixels));
    bool scene_executed = false;
    REQUIRE(pipeline.add_passes(graph, device.screens[0], {}, [&](PassExecuteContext&) { scene_executed = true; }));
    REQUIRE(graph.compile());
    CHECK(graph.execution_order() == std::vector<u32>{2, 1, 0, 3});
    CHECK(graph.dependencies()[1] == std::vector<u32>{2});
    CHECK(graph.dependencies()[0] == std::vector<u32>{1});
    REQUIRE(graph.final_barriers().size() == 1);
    CHECK(graph.final_barriers()[0].after == ResourceAccess::HostRead);
    REQUIRE(pipeline.prepare_resources(graph));
    REQUIRE(graph.execute(device));
    device.serial    = 0;
    device.completed = 1;
    CHECK(scene_executed);
    CHECK(device.draws == 2);
    bool sampled_hdr = false;
    for (const auto& batch : device.barrier_batches)
    {
        for (const auto& barrier : batch.textures)
        {
            sampled_hdr |= barrier.access == ResourceAccess::ShaderRead;
        }
    }
    CHECK(sampled_hdr);
    const auto idle = device.idle_count;
    REQUIRE(pipeline.read_pixels(device.screens[0], pixels));
    CHECK(device.idle_count == idle + 1);
    CHECK(pixels == std::vector<u8>(32 * 24 * 4, 0x7f));
    CHECK_FALSE(pipeline.read_pixels(device.screens[1], pixels));
    graph.reset();
    device.copy_succeeds = false;
    device.serial        = 2;
    REQUIRE(pipeline.add_passes(graph, device.screens[0], {}, {}));
    REQUIRE(graph.compile());
    REQUIRE(pipeline.prepare_resources(graph));
    REQUIRE(graph.execute(device));
    device.serial    = 0;
    device.completed = 2;
    CHECK_FALSE(pipeline.read_pixels(device.screens[0], pixels));
}

TEST_CASE("PostProcessPipeline: invalid extents and exposure leave the graph unchanged")
{
    PostProcessDevice   device;
    PostProcessPipeline pipeline(device);
    CHECK_FALSE(pipeline.prepare());
    REQUIRE(initialize(pipeline));
    device.width = 0;
    CHECK_FALSE(pipeline.prepare());
    device.width = 32;
    REQUIRE(pipeline.prepare());
    FrameGraph graph;
    CHECK_FALSE(pipeline.add_passes(graph, 999, {}, {}));
    CHECK_FALSE(pipeline.add_passes(graph, device.screens[0], {}, {}, {-1.0f, true}));
    CHECK_FALSE(pipeline.add_passes(graph, device.screens[0], {}, {}, {std::numeric_limits<f32>::infinity(), true}));
    CHECK(graph.pass_count() == 0);
}
