#pragma once

#include "tests/mocks/mock_graphics_device.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <unordered_map>

namespace rover
{
    // Tracks ownership and references so teardown checks cover Vulkan lifetime
    // requirements, including rollback after a partially allocated resize.
    class MockResourceGraphicsDevice : public MockGraphicsDevice
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

        u64 serial = 1, completed = 0;

        u64 recording_serial() const override { return serial; }

        u64 completed_serial() override { return completed; }

        void wait_idle() override
        {
            ++idle_count;
            completed = std::max(completed, serial ? serial - 1 : 0);
        }

        u32 get_swapchain_width() override { return width; }

        u32 get_swapchain_height() override { return height; }

        u32 get_swapchain_image_count() override { return static_cast<u32>(screens.size()); }

        TextureHandle get_swapchain_texture_at(u32 i) override { return screens.at(i); }

        Format get_swapchain_format() override { return format; }
    };

} // namespace rover
