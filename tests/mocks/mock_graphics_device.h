#pragma once

#include "core/graphics/graphics_device.h"

#include <string>
#include <vector>

namespace rover
{

    // Null device with command events; resource tests override allocation methods.
    class MockGraphicsDevice : public GraphicsDevice
    {
    public:
        std::vector<std::string> events;

        struct BarrierBatch
        {
            std::vector<BufferBarrier>  buffers;
            std::vector<TextureBarrier> textures;
        };

        std::vector<BarrierBatch> barrier_batches;
        bool                      barriers_succeed = true;

        bool init(WindowSystem&) override { return true; }

        void shutdown() override {}

        BufferHandle create_buffer(const BufferDesc&) override { return INVALID_HANDLE; }

        void destroy_buffer(BufferHandle) override {}

        void* map_buffer(BufferHandle) override { return nullptr; }

        void unmap_buffer(BufferHandle) override {}

        void update_buffer(BufferHandle, const void*, usize, usize) override {}

        TextureHandle create_texture(const TextureDesc&) override { return INVALID_HANDLE; }

        void destroy_texture(TextureHandle) override {}

        void update_texture(TextureHandle,
                            const void*,
                            usize,
                            TextureSubresourceRange = TEXTURE_BASE_SUBRESOURCE) override
        {}

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

        bool cmd_resource_barriers(CommandListHandle,
                                   std::span<const BufferBarrier>  buffers,
                                   std::span<const TextureBarrier> textures) override
        {
            events.emplace_back("barriers");
            barrier_batches.push_back({{buffers.begin(), buffers.end()}, {textures.begin(), textures.end()}});
            return barriers_succeed;
        }

        bool cmd_copy_texture_to_buffer(CommandListHandle, TextureHandle, BufferHandle, TextureCopyRegion = {}) override
        {
            events.emplace_back("copy_texture_to_buffer");
            return true;
        }

        bool cmd_clear_texture(CommandListHandle,
                               TextureHandle,
                               const ClearValue&,
                               TextureSubresourceRange = {}) override
        {
            events.emplace_back("clear_texture");
            return true;
        }

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

        u64 recording_serial() const override { return 1; }

        u64 completed_serial() override { return 0; }

        TextureHandle get_swapchain_texture() override { return INVALID_HANDLE; }

        u32 get_swapchain_image_count() override { return 0; }

        TextureHandle get_swapchain_texture_at(u32) override { return INVALID_HANDLE; }

        Format get_swapchain_format() override { return Format::UNDEFINED; }

        u32 get_swapchain_width() override { return 0; }

        u32 get_swapchain_height() override { return 0; }

        const char* get_device_name() const override { return "mock"; }

        const char* get_api_name() const override { return "mock"; }
    };

} // namespace rover
