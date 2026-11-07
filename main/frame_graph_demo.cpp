#include "main/frame_graph_demo.h"

#include "core/graphics/graphics_device.h"
#include "core/log/log.h"
#include "platform/linux/event_pump.h"
#include "platform/linux/window.h"
#include "services/graphics/frame_graph.h"

#include <array>

namespace rover
{
    namespace
    {
        constexpr u32   kWidth  = 64;
        constexpr u32   kHeight = 64;
        constexpr usize kBytes  = kWidth * kHeight * 4;

        RenderPassHandle make_color_pass(GraphicsDevice& device, Format format)
        {
            RenderPassDesc      desc{};
            ColorAttachmentDesc color{};
            color.format = format;
            desc.color_attachments.push_back(color);
            return device.create_render_pass(desc);
        }

        struct DemoResources
        {
            GraphicsDevice&             device;
            TextureHandle               image = INVALID_HANDLE;
            std::array<BufferHandle, 2> readback{};
            RenderPassHandle            offscreen_pass = INVALID_HANDLE;
            FramebufferHandle           offscreen_fb   = INVALID_HANDLE;
            RenderPassHandle            present_pass   = INVALID_HANDLE;
            FramebufferHandle           present_fb     = INVALID_HANDLE;

            ~DemoResources()
            {
                device.wait_idle();
                clear_present();
                device.destroy_framebuffer(offscreen_fb);
                device.destroy_render_pass(offscreen_pass);
                device.destroy_texture(image);
                for (auto buffer : readback)
                {
                    device.destroy_buffer(buffer);
                }
            }

            void clear_present()
            {
                device.destroy_framebuffer(present_fb);
                device.destroy_render_pass(present_pass);
                present_fb   = INVALID_HANDLE;
                present_pass = INVALID_HANDLE;
            }

            bool init()
            {
                TextureDesc texture{};
                texture.width  = kWidth;
                texture.height = kHeight;
                texture.format = Format::R8G8B8A8_UNORM;
                texture.usage  = TextureUsage::ColorAttachment | TextureUsage::TransferSrc | TextureUsage::Sampled;
                image          = device.create_texture(texture);
                for (auto& buffer : readback)
                {
                    buffer = device.create_buffer(
                        {kBytes, BufferUsage::Transfer, MemoryUsage::GpuToCpu, "version readback"});
                }
                offscreen_pass = make_color_pass(device, texture.format);
                if (image == INVALID_HANDLE || readback[0] == INVALID_HANDLE || readback[1] == INVALID_HANDLE ||
                    offscreen_pass == INVALID_HANDLE)
                {
                    return false;
                }
                FramebufferDesc fb{};
                fb.render_pass       = offscreen_pass;
                fb.color_attachments = {image};
                fb.width             = kWidth;
                fb.height            = kHeight;
                offscreen_fb         = device.create_framebuffer(fb);
                return offscreen_fb != INVALID_HANDLE;
            }

            // This validation demo waits for GPU completion every frame. Rebuild
            // per acquired image to cover swapchain recreation without stale IDs.
            bool prepare_present()
            {
                clear_present();
                present_pass = make_color_pass(device, device.get_swapchain_format());
                if (present_pass == INVALID_HANDLE)
                {
                    return false;
                }
                FramebufferDesc fb{};
                fb.render_pass       = present_pass;
                fb.color_attachments = {device.get_swapchain_texture()};
                fb.width             = device.get_swapchain_width();
                fb.height            = device.get_swapchain_height();
                present_fb           = device.create_framebuffer(fb);
                return present_fb != INVALID_HANDLE;
            }
        };
    } // namespace

    int run_frame_graph_demo(GraphicsDevice& device, Window& window, EventPump& pump, u32 frame_limit)
    {
        DemoResources resources{device};
        if (!resources.init())
        {
            ROVER_LOG_ERROR("Frame Graph demo: unable to create readback resources");
            return 1;
        }
        const u32 target_frames   = frame_limit == 0 ? 8 : frame_limit;
        u32       verified_frames = 0;
        while (!window.should_close() && verified_frames < target_frames)
        {
            pump.poll();
            if (!device.begin_frame())
            {
                continue;
            }
            if (!resources.prepare_present())
            {
                device.end_frame();
                device.present();
                ROVER_LOG_ERROR("Frame Graph demo: unable to create presentation framebuffer");
                return 1;
            }
            const std::array<u8, 4>                blue{32, 96, 192, 255};
            const std::array<u8, 4>                red{192, 64, 16, 255};
            const std::array<std::array<u8, 4>, 2> expected =
                verified_frames % 2 == 0 ? std::array{blue, red} : std::array{red, blue};
            std::array<ClearValue, 2> clears{};
            for (usize version = 0; version < expected.size(); ++version)
            {
                for (usize component = 0; component < 4; ++component)
                {
                    clears[version].color[component] = static_cast<f32>(expected[version][component]) / 255.0f;
                }
            }
            FrameGraph graph;
            const auto image = graph.import_texture(
                "offscreen.v0", resources.image, Format::R8G8B8A8_UNORM, kWidth, kHeight, resources.offscreen_fb);
            const auto overwritten = graph.create_version(image, "offscreen.v1");
            const auto screen      = graph.import_texture("screen",
                                                          device.get_swapchain_texture(),
                                                          device.get_swapchain_format(),
                                                          device.get_swapchain_width(),
                                                          device.get_swapchain_height(),
                                                          resources.present_fb);
            graph.set_final_access(overwritten, ResourceAccess::ShaderRead);
            std::array<bool, 2> copied{};
            auto                add_readback = [&](usize version, RenderResourceId source) {
                const auto buffer =
                    graph.import_buffer("readback.v" + std::to_string(version), resources.readback[version]);
                graph.set_final_access(buffer, ResourceAccess::HostRead);
                graph.add_pass(
                    "readback.v" + std::to_string(version),
                    [=](PassBuilder& b) {
                        b.read(source, ResourceAccess::TransferRead);
                        b.write(buffer, ResourceAccess::TransferWrite);
                    },
                    [&, version, source](PassExecuteContext& ctx) {
                        copied[version] = ctx.device.cmd_copy_texture_to_buffer(
                            ctx.cmd, ctx.graph.resource(source).texture, resources.readback[version]);
                    });
            };
            // Register the overwrite before the old reader: WAR must move that
            // reader ahead of the overwrite, while RAW finds both producers.
            add_readback(1, overwritten);
            const auto overwrite = graph.add_pass("overwrite.v1", {}, {});
            graph.set_color_attachment(overwrite, overwritten, clears[1]);
            add_readback(0, image);
            const auto producer = graph.add_pass("clear.v0", {}, {});
            graph.set_color_attachment(producer, image, clears[0]);
            const auto present = graph.add_pass("screen clear", {}, {});
            graph.set_color_attachment(present, screen, clears[1]);
            const bool recorded = graph.compile() && graph.execute(device);
            device.end_frame();
            device.present();
            device.wait_idle();
            if (!recorded || !copied[0] || !copied[1])
            {
                ROVER_LOG_ERROR("Frame Graph demo: failed to record the readback graph");
                return 1;
            }
            for (usize version = 0; version < expected.size(); ++version)
            {
                const auto* pixels = static_cast<const u8*>(device.map_buffer(resources.readback[version]));
                if (pixels == nullptr)
                {
                    ROVER_LOG_ERROR("Frame Graph demo: could not map version {} readback", version);
                    return 1;
                }
                bool valid = true;
                for (usize byte = 0; byte < kBytes; ++byte)
                {
                    if (pixels[byte] != expected[version][byte % 4])
                    {
                        ROVER_LOG_ERROR("Frame Graph demo: frame {} version {} byte {} expected {}, got {}",
                                        verified_frames,
                                        version,
                                        byte,
                                        expected[version][byte % 4],
                                        pixels[byte]);
                        valid = false;
                        break;
                    }
                }
                device.unmap_buffer(resources.readback[version]);
                if (!valid)
                {
                    return 1;
                }
            }
            ++verified_frames;
        }
        if (verified_frames != target_frames)
        {
            ROVER_LOG_ERROR("Frame Graph demo: window closed after only {} verified frames", verified_frames);
            return 1;
        }
        ROVER_LOG_INFO("Frame Graph GPU readback verified: {} frames, 2 versions per frame, {} pixels per version",
                       verified_frames,
                       kWidth * kHeight);
        return 0;
    }
} // namespace rover
