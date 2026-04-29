// Unit tests for services/graphics/frame_graph.
//
// FrameGraph itself is decoupled from the GPU; the tests verify resource
// import + pass declaration + execute order without instantiating a real
// GraphicsDevice. A null-op mock device is used to exercise execute().

#include "services/graphics/frame_graph.h"

#include <doctest/doctest.h>

#include <vector>

using namespace rover;

namespace
{

    // Minimal mock GraphicsDevice that records the order of pass commands. We
    // only override the methods exercised by FrameGraph::execute().
    class MockDevice : public GraphicsDevice
    {
    public:
        std::vector<std::string> events;

        bool init(WindowSystem&) override { return true; }

        void shutdown() override {}

        BufferHandle create_buffer(const BufferDesc&) override { return INVALID_HANDLE; }

        void destroy_buffer(BufferHandle) override {}

        void* map_buffer(BufferHandle) override { return nullptr; }

        void unmap_buffer(BufferHandle) override {}

        void update_buffer(BufferHandle, const void*, usize, usize) override {}

        TextureHandle create_texture(const TextureDesc&) override { return INVALID_HANDLE; }

        void destroy_texture(TextureHandle) override {}

        void update_texture(TextureHandle, const void*, usize) override {}

        SamplerHandle create_sampler(const SamplerDesc&) override { return INVALID_HANDLE; }

        void destroy_sampler(SamplerHandle) override {}

        ShaderHandle create_shader(const ShaderDesc&) override { return INVALID_HANDLE; }

        void destroy_shader(ShaderHandle) override {}

        RenderPassHandle create_render_pass(const RenderPassDesc&) override { return INVALID_HANDLE; }

        void destroy_render_pass(RenderPassHandle) override {}

        FramebufferHandle create_framebuffer(const FramebufferDesc&) override { return INVALID_HANDLE; }

        void destroy_framebuffer(FramebufferHandle) override {}

        BindGroupLayoutHandle create_bind_group_layout(const BindGroupLayoutDesc&) override { return INVALID_HANDLE; }

        void destroy_bind_group_layout(BindGroupLayoutHandle) override {}

        BindGroupHandle create_bind_group(const BindGroupDesc&) override { return INVALID_HANDLE; }

        void destroy_bind_group(BindGroupHandle) override {}

        PipelineLayoutHandle create_pipeline_layout(const PipelineLayoutDesc&) override { return INVALID_HANDLE; }

        void destroy_pipeline_layout(PipelineLayoutHandle) override {}

        PipelineHandle create_graphics_pipeline(const GraphicsPipelineDesc&) override { return INVALID_HANDLE; }

        void destroy_pipeline(PipelineHandle) override {}

        CommandListHandle begin_command_list() override
        {
            events.emplace_back("begin_cmd");
            return 1;
        }

        void end_command_list(CommandListHandle) override { events.emplace_back("end_cmd"); }

        void cmd_begin_render_pass(CommandListHandle, FramebufferHandle, const ClearValue*, u32) override
        {
            events.emplace_back("begin_pass");
        }

        void cmd_end_render_pass(CommandListHandle) override { events.emplace_back("end_pass"); }

        void cmd_bind_pipeline(CommandListHandle, PipelineHandle) override {}

        void cmd_set_viewport(CommandListHandle, const Viewport&) override {}

        void cmd_set_scissor(CommandListHandle, const Scissor&) override {}

        void cmd_bind_group(CommandListHandle, u32, BindGroupHandle) override {}

        void cmd_push_constants(CommandListHandle, PipelineLayoutHandle, ShaderStage, const void*, u32, u32) override {}

        void cmd_bind_vertex_buffer(CommandListHandle, BufferHandle, u32, usize) override {}

        void cmd_bind_index_buffer(CommandListHandle, BufferHandle, IndexType, usize) override {}

        void cmd_draw(CommandListHandle, u32, u32, u32, u32) override {}

        void cmd_draw_indexed(CommandListHandle, u32, u32, u32, i32, u32) override {}

        bool begin_frame() override { return true; }

        void end_frame() override {}

        void present() override {}

        void wait_idle() override {}

        TextureHandle get_swapchain_texture() override { return INVALID_HANDLE; }

        u32 get_swapchain_image_count() override { return 0; }

        TextureHandle get_swapchain_texture_at(u32) override { return INVALID_HANDLE; }

        Format get_swapchain_format() override { return Format::UNDEFINED; }

        u32 get_swapchain_width() override { return 0; }

        u32 get_swapchain_height() override { return 0; }

        const char* get_device_name() const override { return "mock"; }

        const char* get_api_name() const override { return "mock"; }
    };

} // namespace

TEST_CASE("FrameGraph: imported texture is recorded with metadata")
{
    FrameGraph fg;
    auto       id = fg.import_texture("color", 0xCAFE, Format::R8G8B8A8_SRGB, 800, 600);
    CHECK(id != INVALID_RESOURCE_ID);
    CHECK(fg.resource(id).name == "color");
    CHECK(fg.resource(id).texture == 0xCAFE);
    CHECK(fg.resource(id).width == 800);
    CHECK(fg.resource(id).format == Format::R8G8B8A8_SRGB);
}

TEST_CASE("FrameGraph: passes execute in declaration order")
{
    FrameGraph fg;
    auto       color = fg.import_texture("color", 1, Format::B8G8R8A8_UNORM, 1, 1);

    std::vector<int> order;
    fg.add_pass("a", [&](PassBuilder& b) { b.write(color); }, [&](PassExecuteContext&) { order.push_back(1); });
    fg.add_pass("b", [&](PassBuilder& b) { b.read(color); }, [&](PassExecuteContext&) { order.push_back(2); });

    REQUIRE(fg.compile());

    MockDevice dev;
    fg.execute(dev);

    CHECK(order == std::vector<int>{1, 2});
    // Each pass: begin_cmd + execute + end_cmd (no begin_pass since
    // framebuffer wasn't set on either pass).
    CHECK(dev.events.size() == 4);
}

TEST_CASE("FrameGraph: set_color_attachment wraps execute with render pass")
{
    FrameGraph fg;
    auto       color = fg.import_texture("color", 1, Format::B8G8R8A8_UNORM, 1, 1, /*fb*/ 0xF00);

    fg.add_pass("a", [&](PassBuilder& b) { b.write(color); }, [&](PassExecuteContext&) {});
    ClearValue clear{};
    fg.set_color_attachment(0, color, clear);

    REQUIRE(fg.compile());

    MockDevice dev;
    fg.execute(dev);

    REQUIRE(dev.events.size() == 4);
    CHECK(dev.events[0] == "begin_cmd");
    CHECK(dev.events[1] == "begin_pass");
    CHECK(dev.events[2] == "end_pass");
    CHECK(dev.events[3] == "end_cmd");
}

TEST_CASE("FrameGraph: compile fails when a pass references a bad resource id")
{
    FrameGraph fg;
    fg.add_pass("bad", [](PassBuilder& b) { b.write(static_cast<RenderResourceId>(99)); }, [](PassExecuteContext&) {});
    CHECK_FALSE(fg.compile());
}

TEST_CASE("FrameGraph: reset clears resources and passes")
{
    FrameGraph fg;
    fg.import_texture("a", 1, Format::R8G8B8A8_UNORM, 1, 1);
    fg.add_pass("p", [](PassBuilder&) {}, [](PassExecuteContext&) {});
    fg.reset();
    CHECK(fg.pass_count() == 0);
    CHECK(fg.resource_count() == 0);
}
