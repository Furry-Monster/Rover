#include "main/subresource_demo.h"

#include "core/graphics/graphics_device.h"
#include "core/log/log.h"
#include "platform/linux/event_pump.h"
#include "platform/linux/window.h"
#include "services/graphics/frame_graph.h"
#include "services/graphics/transient_resource_pool.h"
#include "shaders/post_process_vert.spv.h"
#include "shaders/present_frag.spv.h"

#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace rover
{
    namespace
    {
        using Color                               = std::array<u8, 4>;
        constexpr u32                     kSize   = 16;
        constexpr u32                     kMips   = 3;
        constexpr u32                     kLayers = 2;
        constexpr TextureSubresourceRange kSelected{TextureAspect::Color, 1, 1, 1, 1};
        constexpr Color                   kGreen{64, 224, 96, 255};
        constexpr Color                   kAttachment0{200, 40, 90, 255};
        constexpr Color                   kAttachment1{30, 180, 220, 255};

        TextureSubresourceRange slice(u32 mip, u32 layer)
        {
            return {TextureAspect::Color, mip, 1, layer, 1};
        }

        Color initial(u32 mip, u32 layer)
        {
            return {static_cast<u8>(20 + 30 * mip),
                    static_cast<u8>(40 + 60 * layer),
                    static_cast<u8>(50 + 20 * mip + 20 * layer),
                    255};
        }

        ClearValue clear_color(const Color& color)
        {
            ClearValue value{};
            for (usize i = 0; i < 4; ++i)
            {
                value.color[i] = static_cast<f32>(color[i]) / 255.0f;
            }
            return value;
        }

        std::vector<u8> pixels(const Color& color, u32 extent)
        {
            std::vector<u8> bytes(extent * extent * 4);
            for (usize i = 0; i < bytes.size(); ++i)
            {
                bytes[i] = color[i % 4];
            }
            return bytes;
        }

        template <usize N>
        ShaderDesc shader(ShaderStage stage, const std::uint32_t (&data)[N])
        {
            return {stage, reinterpret_cast<const u8*>(data), sizeof(data), "main"};
        }

        RenderPassHandle color_pass(GraphicsDevice& device, Format format)
        {
            RenderPassDesc      desc{};
            ColorAttachmentDesc color{};
            color.format           = format;
            desc.color_attachments = {color};
            return device.create_render_pass(desc);
        }

        struct Resources
        {
            explicit Resources(GraphicsDevice& graphics) : device(graphics), pool(graphics) {}

            GraphicsDevice&       device;
            TransientResourcePool pool;
            TextureDesc           desc{};
            TextureDesc           ds_desc{};
            TextureHandle         array = 0, depth = 0;
            ShaderHandle          vertex = 0, fragment = 0;
            SamplerHandle         sampler  = 0;
            BindGroupLayoutHandle bindings = 0;
            PipelineLayoutHandle  layout   = 0;
            RenderPassHandle      color = 0, screen = 0;
            PipelineHandle        pipeline = 0;

            ~Resources()
            {
                device.wait_idle();
                pool.clear();
                device.destroy_pipeline(pipeline);
                device.destroy_pipeline_layout(layout);
                device.destroy_bind_group_layout(bindings);
                device.destroy_sampler(sampler);
                device.destroy_shader(vertex);
                device.destroy_shader(fragment);
                device.destroy_render_pass(color);
                device.destroy_render_pass(screen);
                device.destroy_texture(array);
                device.destroy_texture(depth);
            }

            bool init()
            {
                desc.width = desc.height = kSize;
                desc.mip_levels          = kMips;
                desc.array_layers        = kLayers;
                desc.type                = TextureType::Texture2DArray;
                desc.format              = Format::R8G8B8A8_UNORM;
                desc.usage    = TextureUsage::ColorAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
                array         = device.create_texture(desc);
                ds_desc       = desc;
                ds_desc.width = ds_desc.height = 8;
                ds_desc.mip_levels             = 1;
                ds_desc.format                 = Format::D32_SFLOAT_S8_UINT;
                ds_desc.usage                  = TextureUsage::DepthStencilAttachment | TextureUsage::TransferSrc;
                depth                          = device.create_texture(ds_desc);
                vertex   = device.create_shader(shader(ShaderStage::Vertex, post_process_vert_spv));
                fragment = device.create_shader(shader(ShaderStage::Fragment, present_frag_spv));
                sampler  = device.create_sampler({Filter::Nearest,
                                                  Filter::Nearest,
                                                  SamplerAddressMode::ClampToEdge,
                                                  SamplerAddressMode::ClampToEdge,
                                                  SamplerAddressMode::ClampToEdge});
                BindGroupLayoutDesc binding_desc{};
                binding_desc.entries = {{0, BindingType::SampledTexture, ShaderStage::Fragment},
                                        {1, BindingType::Sampler, ShaderStage::Fragment}};
                bindings             = device.create_bind_group_layout(binding_desc);
                PipelineLayoutDesc layout_desc{};
                layout_desc.bind_group_layouts = {bindings};
                layout                         = device.create_pipeline_layout(layout_desc);
                color                          = color_pass(device, desc.format);
                screen                         = color_pass(device, device.get_swapchain_format());
                if (!array || !depth || !vertex || !fragment || !sampler || !bindings || !layout || !color || !screen)
                {
                    return false;
                }
                GraphicsPipelineDesc pipeline_desc{};
                pipeline_desc.vertex_shader   = vertex;
                pipeline_desc.fragment_shader = fragment;
                pipeline_desc.render_pass     = color;
                pipeline_desc.pipeline_layout = layout;
                pipeline_desc.cull_mode       = CullMode::None;
                pipeline                      = device.create_graphics_pipeline(pipeline_desc);
                for (u32 layer = 0; layer < kLayers; ++layer)
                {
                    for (u32 mip = 0; mip < kMips; ++mip)
                    {
                        const auto bytes = pixels(initial(mip, layer), texture_mip_extent(kSize, mip));
                        device.update_texture(array, bytes.data(), bytes.size(), slice(mip, layer));
                    }
                }
                return pipeline != INVALID_HANDLE;
            }
        };

        bool verify_color(GraphicsDevice& device, BufferHandle buffer, usize offset, u32 extent, const Color& expected)
        {
            const auto* data = static_cast<const u8*>(device.map_buffer(buffer));
            if (!data)
            {
                return false;
            }
            bool valid = true;
            for (usize i = 0; i < static_cast<usize>(extent) * extent * 4; ++i)
            {
                {
                    if (std::abs(static_cast<int>(data[offset + i]) - expected[i % 4]) > 1)
                    {
                        ROVER_LOG_ERROR("Subresource color mismatch: offset {}, byte {}, expected {}, received {}",
                                        offset,
                                        i,
                                        expected[i % 4],
                                        data[offset + i]);
                        valid = false;
                        break;
                    }
                }
            }
            device.unmap_buffer(buffer);
            return valid;
        }
    } // namespace

    int run_subresource_demo(GraphicsDevice& device, Window& window, EventPump& pump, u32 frame_limit)
    {
        Resources resources(device);
        if (!resources.init())
        {
            return 1;
        }
        const u32 frames   = frame_limit ? frame_limit : 8;
        u32       verified = 0;
        while (!window.should_close() && verified < frames)
        {
            pump.poll();
            const Color uploaded = verified % 2 == 0 ? Color{32, 96, 192, 255} : Color{192, 64, 16, 255};
            const auto  upload   = pixels(uploaded, kSize / 2);
            device.update_texture(resources.array, upload.data(), upload.size(), kSelected);
            if (!device.begin_frame())
            {
                continue;
            }
            FrameGraph graph;
            const auto array      = graph.import_texture("array", resources.array, resources.desc);
            const auto changed    = graph.create_version(array);
            const auto ds         = graph.import_texture("depth stencil", resources.depth, resources.ds_desc);
            const auto ds_changed = graph.create_version(ds);
            const auto source_copy =
                graph.create_buffer("array planes", {4096, BufferUsage::Transfer, MemoryUsage::GpuToCpu});
            const auto old_copy = graph.create_buffer("old slice", {256, BufferUsage::Transfer, MemoryUsage::GpuToCpu});
            const auto sample_copy =
                graph.create_buffer("sampled slice", {1024, BufferUsage::Transfer, MemoryUsage::GpuToCpu});
            const auto ds_copy =
                graph.create_buffer("depth stencil planes", {1024, BufferUsage::Transfer, MemoryUsage::GpuToCpu});
            auto output_desc       = resources.desc;
            output_desc.mip_levels = output_desc.array_layers = 1;
            output_desc.type                                  = TextureType::Texture2D;
            const auto          output                        = graph.create_texture("sample output", output_desc);
            std::array<bool, 8> commands{};
            graph.add_pass(
                "copy whole new version",
                [=](PassBuilder& b) {
                    b.read(changed, ResourceAccess::TransferRead);
                    b.write(source_copy, ResourceAccess::TransferWrite);
                },
                [&](PassExecuteContext& ctx) {
                    usize offset = 0;
                    commands[0]  = true;
                    for (u32 layer = 0; layer < kLayers; ++layer)
                    {
                        for (u32 mip = 0; mip < kMips; ++mip)
                        {
                            commands[0] &= ctx.device.cmd_copy_texture_to_buffer(ctx.cmd,
                                                                                 resources.array,
                                                                                 ctx.graph.resource(source_copy).buffer,
                                                                                 {slice(mip, layer), offset});
                            const auto extent = texture_mip_extent(kSize, mip);
                            offset += extent * extent * 4;
                        }
                    }
                });
            graph.add_pass(
                "overwrite selected slice",
                [=](PassBuilder& b) { b.write(changed, ResourceAccess::TransferWrite, kSelected); },
                [&](PassExecuteContext& ctx) {
                    commands[1] =
                        ctx.device.cmd_clear_texture(ctx.cmd, resources.array, clear_color(kGreen), kSelected);
                });
            graph.add_pass(
                "copy uploaded old slice",
                [=](PassBuilder& b) {
                    b.read(array, ResourceAccess::TransferRead, kSelected);
                    b.write(old_copy, ResourceAccess::TransferWrite);
                },
                [&](PassExecuteContext& ctx) {
                    commands[2] = ctx.device.cmd_copy_texture_to_buffer(
                        ctx.cmd, resources.array, ctx.graph.resource(old_copy).buffer, {kSelected});
                });
            const auto attachment0 = graph.add_pass("clear mip0 layer0 attachment", {}, {});
            graph.set_render_target(
                attachment0, resources.color, array, clear_color(kAttachment0), INVALID_RESOURCE_ID, slice(0, 0));
            const auto attachment1 = graph.add_pass("clear mip2 layer1 attachment", {}, {});
            graph.set_render_target(
                attachment1, resources.color, array, clear_color(kAttachment1), INVALID_RESOURCE_ID, slice(2, 1));
            RenderBinding image{}, sampler{};
            image.entry.type          = BindingType::SampledTexture;
            image.entry.texture_range = kSelected;
            image.resource            = changed;
            sampler.entry.binding     = 1;
            sampler.entry.type        = BindingType::Sampler;
            sampler.entry.sampler     = resources.sampler;
            const auto bindings       = graph.create_bind_group(resources.bindings, {image, sampler});
            const auto sample         = graph.add_pass(
                "sample mip1 layer1 view",
                [=](PassBuilder& b) { b.read(changed, ResourceAccess::ShaderRead, kSelected); },
                [&](PassExecuteContext& ctx) {
                    ctx.device.cmd_bind_pipeline(ctx.cmd, resources.pipeline);
                    ctx.device.cmd_set_viewport(ctx.cmd, {0, 0, kSize, kSize});
                    ctx.device.cmd_set_scissor(ctx.cmd, {0, 0, kSize, kSize});
                    ctx.device.cmd_bind_group(ctx.cmd, 0, ctx.graph.bind_group(bindings));
                    ctx.device.cmd_draw(ctx.cmd, 3, 1, 0, 0);
                });
            graph.set_render_target(sample, resources.color, output, {});
            graph.add_pass(
                "read sampled view",
                [=](PassBuilder& b) {
                    b.read(output, ResourceAccess::TransferRead);
                    b.write(sample_copy, ResourceAccess::TransferWrite);
                },
                [&](PassExecuteContext& ctx) {
                    commands[3] = ctx.device.cmd_copy_texture_to_buffer(
                        ctx.cmd, ctx.graph.resource(output).texture, ctx.graph.resource(sample_copy).buffer);
                });
            graph.add_pass(
                "initialize both depth stencil layers",
                [=](PassBuilder& b) { b.write(ds, ResourceAccess::TransferWrite); },
                [&](PassExecuteContext& ctx) {
                    ClearValue clear{};
                    clear.depth   = 0.25f;
                    clear.stencil = 77;
                    commands[4]   = ctx.device.cmd_clear_texture(ctx.cmd, resources.depth, clear);
                });
            const TextureSubresourceRange depth0{TextureAspect::Depth, 0, 1, 0, 1};
            const TextureSubresourceRange stencil1{TextureAspect::Stencil, 0, 1, 1, 1};
            graph.add_pass(
                "overwrite only depth layer0",
                [=](PassBuilder& b) { b.write(ds_changed, ResourceAccess::TransferWrite, depth0); },
                [&, depth0](PassExecuteContext& ctx) {
                    ClearValue clear{};
                    clear.depth = 0.75f;
                    commands[5] = ctx.device.cmd_clear_texture(ctx.cmd, resources.depth, clear, depth0);
                });
            graph.add_pass(
                "overwrite only stencil layer1",
                [=](PassBuilder& b) { b.write(ds_changed, ResourceAccess::TransferWrite, stencil1); },
                [&, stencil1](PassExecuteContext& ctx) {
                    ClearValue clear{};
                    clear.stencil = 17;
                    commands[6]   = ctx.device.cmd_clear_texture(ctx.cmd, resources.depth, clear, stencil1);
                });
            graph.add_pass(
                "copy depth stencil planes",
                [=](PassBuilder& b) {
                    b.read(ds_changed, ResourceAccess::TransferRead);
                    b.write(ds_copy, ResourceAccess::TransferWrite);
                },
                [&](PassExecuteContext& ctx) {
                    commands[7] = true;
                    for (u32 layer = 0; layer < 2; ++layer)
                    {
                        for (u32 plane = 0; plane < 2; ++plane)
                        {
                            const TextureSubresourceRange range{
                                plane == 0 ? TextureAspect::Depth : TextureAspect::Stencil, 0, 1, layer, 1};
                            commands[7] &= ctx.device.cmd_copy_texture_to_buffer(
                                ctx.cmd,
                                resources.depth,
                                ctx.graph.resource(ds_copy).buffer,
                                {range, static_cast<usize>(layer * 512 + plane * 256)});
                        }
                    }
                });
            const auto screen  = graph.import_texture("screen",
                                                      device.get_swapchain_texture(),
                                                      device.get_swapchain_format(),
                                                      device.get_swapchain_width(),
                                                      device.get_swapchain_height());
            const auto present = graph.add_pass("screen", {}, {});
            graph.set_render_target(present, resources.screen, screen, clear_color(kGreen));
            for (const auto buffer : {source_copy, old_copy, sample_copy, ds_copy})
            {
                graph.set_final_access(buffer, ResourceAccess::HostRead);
            }
            const bool recorded = graph.compile() && graph.prepare_resources(resources.pool) && graph.execute(device);
            device.end_frame();
            device.present();
            device.wait_idle();
            if (!recorded)
            {
                return 1;
            }
            for (bool command : commands)
            {
                if (!command)
                {
                    return 1;
                }
            }
            usize offset = 0;
            for (u32 layer = 0; layer < kLayers; ++layer)
            {
                for (u32 mip = 0; mip < kMips; ++mip)
                {
                    auto expected = initial(mip, layer);
                    if (mip == 1 && layer == 1)
                    {
                        expected = kGreen;
                    }
                    if (mip == 0 && layer == 0)
                    {
                        expected = kAttachment0;
                    }
                    if (mip == 2 && layer == 1)
                    {
                        expected = kAttachment1;
                    }
                    const auto extent = texture_mip_extent(kSize, mip);
                    if (!verify_color(device, graph.resource(source_copy).buffer, offset, extent, expected))
                    {
                        return 1;
                    }
                    offset += extent * extent * 4;
                }
            }
            if (!verify_color(device, graph.resource(old_copy).buffer, 0, kSize / 2, uploaded) ||
                !verify_color(device, graph.resource(sample_copy).buffer, 0, kSize, kGreen))
            {
                return 1;
            }
            const auto* bytes = static_cast<const u8*>(device.map_buffer(graph.resource(ds_copy).buffer));
            if (!bytes)
            {
                return 1;
            }
            bool valid = true;
            for (u32 layer = 0; layer < 2; ++layer)
            {
                for (usize pixel = 0; pixel < 64; ++pixel)
                {
                    f32 depth = 0;
                    std::memcpy(&depth, bytes + layer * 512 + pixel * 4, 4);
                    valid &= std::abs(depth - (layer == 0 ? 0.75f : 0.25f)) < 0.00001f;
                    valid &= bytes[layer * 512 + 256 + pixel] == (layer == 0 ? 77 : 17);
                }
            }
            device.unmap_buffer(graph.resource(ds_copy).buffer);
            if (!valid)
            {
                return 1;
            }
            ++verified;
        }
        if (verified != frames)
        {
            return 1;
        }
        ROVER_LOG_INFO("Subresources GPU verified: {} frames, 3 mips x 2 layers, upload/attachment/sample views, "
                       "independent depth/stencil planes",
                       verified);
        return 0;
    }
} // namespace rover
