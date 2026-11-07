#include "services/graphics/post_process_pipeline.h"
#include "tests/mocks/mock_graphics_device.h"

#include <doctest/doctest.h>

#include <limits>
#include <unordered_map>
#include <unordered_set>

using namespace rover;

namespace
{
    // Tracks ownership and references so teardown checks cover Vulkan lifetime
    // requirements, including rollback after a partially allocated resize.
    class PostProcessDevice : public MockGraphicsDevice
    {
    public:
        u32                                       width = 32, height = 24, idle_count = 0, draws = 0;
        std::vector<TextureHandle>                screens{1001, 1002};
        Format                                    format = Format::B8G8R8A8_SRGB;
        u64                                       next = 1, fail_at = 0;
        bool                                      copy_succeeds = true;
        std::unordered_map<u64, std::vector<u64>> live;
        std::unordered_map<u64, TextureDesc>      textures;
        std::unordered_map<u64, std::vector<u8>>  buffers;
        std::vector<BindGroupLayoutEntry>         layout_entries;

        u64 allocate(std::vector<u64> dependencies = {})
        {
            const auto id = next++;
            if (id == fail_at)
            {
                return INVALID_HANDLE;
            }
            live.emplace(id, std::move(dependencies));
            return id;
        }

        void release(u64 id)
        {
            for (const auto& [owner, dependencies] : live)
            {
                for (const auto dependency : dependencies)
                {
                    CHECK(dependency != id);
                }
            }
            CHECK(live.erase(id) == 1);
        }

        ShaderHandle create_shader(const ShaderDesc&) override { return allocate(); }

        void destroy_shader(ShaderHandle h) override { release(h); }

        SamplerHandle create_sampler(const SamplerDesc&) override { return allocate(); }

        void destroy_sampler(SamplerHandle h) override { release(h); }

        RenderPassHandle create_render_pass(const RenderPassDesc&) override { return allocate(); }

        void destroy_render_pass(RenderPassHandle h) override { release(h); }

        BindGroupLayoutHandle create_bind_group_layout(const BindGroupLayoutDesc& d) override
        {
            layout_entries = d.entries;
            return allocate();
        }

        void destroy_bind_group_layout(BindGroupLayoutHandle h) override { release(h); }

        PipelineLayoutHandle create_pipeline_layout(const PipelineLayoutDesc& d) override
        {
            return allocate({d.bind_group_layouts.begin(), d.bind_group_layouts.end()});
        }

        void destroy_pipeline_layout(PipelineLayoutHandle h) override { release(h); }

        PipelineHandle create_graphics_pipeline(const GraphicsPipelineDesc& d) override
        {
            return allocate({d.vertex_shader, d.fragment_shader, d.render_pass, d.pipeline_layout});
        }

        void destroy_pipeline(PipelineHandle h) override { release(h); }

        TextureHandle create_texture(const TextureDesc& d) override
        {
            const auto h = allocate();
            if (h)
            {
                textures.emplace(h, d);
            }
            return h;
        }

        void destroy_texture(TextureHandle h) override
        {
            release(h);
            textures.erase(h);
        }

        FramebufferHandle create_framebuffer(const FramebufferDesc& d) override
        {
            auto dependencies = d.color_attachments;
            dependencies.push_back(d.render_pass);
            if (d.depth_stencil)
            {
                dependencies.push_back(d.depth_stencil);
            }
            return allocate(std::move(dependencies));
        }

        void destroy_framebuffer(FramebufferHandle h) override { release(h); }

        BindGroupHandle create_bind_group(const BindGroupDesc& d) override
        {
            std::vector<u64> dependencies{d.layout};
            for (const auto& entry : d.entries)
            {
                dependencies.push_back(entry.type == BindingType::Sampler ? entry.sampler : entry.texture);
            }
            return allocate(std::move(dependencies));
        }

        void destroy_bind_group(BindGroupHandle h) override { release(h); }

        BufferHandle create_buffer(const BufferDesc& d) override
        {
            const auto h = allocate();
            if (h)
            {
                buffers.emplace(h, std::vector<u8>(d.size, 0x7f));
            }
            return h;
        }

        void destroy_buffer(BufferHandle h) override
        {
            release(h);
            buffers.erase(h);
        }

        void* map_buffer(BufferHandle h) override { return buffers.at(h).data(); }

        bool cmd_copy_texture_to_buffer(CommandListHandle, TextureHandle, BufferHandle) override
        {
            return copy_succeeds;
        }

        void cmd_draw(CommandListHandle, u32 vertices, u32 instances, u32, u32) override
        {
            CHECK(vertices == 3);
            CHECK(instances == 1);
            ++draws;
        }

        void wait_idle() override { ++idle_count; }

        u32 get_swapchain_width() override { return width; }

        u32 get_swapchain_height() override { return height; }

        u32 get_swapchain_image_count() override { return static_cast<u32>(screens.size()); }

        TextureHandle get_swapchain_texture_at(u32 i) override { return screens.at(i); }

        Format get_swapchain_format() override { return format; }
    };

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
        CHECK(device.textures.size() == 6);
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
        CHECK(device.textures.size() == 9);
    }
    CHECK(device.live.empty());
}

TEST_CASE("PostProcessPipeline: allocation failure rolls back every partial target")
{
    // Fail each allocation in a full rebuild, including the second image.
    for (u64 offset = 0; offset < 20; ++offset)
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
    REQUIRE(graph.execute(device));
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
    REQUIRE(pipeline.add_passes(graph, device.screens[0], {}, {}));
    REQUIRE(graph.compile());
    REQUIRE(graph.execute(device));
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
