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
            GraphicsDevice&   device;
            TextureHandle     image          = INVALID_HANDLE;
            BufferHandle      readback       = INVALID_HANDLE;
            RenderPassHandle  offscreen_pass = INVALID_HANDLE;
            FramebufferHandle offscreen_fb   = INVALID_HANDLE;
            RenderPassHandle  present_pass   = INVALID_HANDLE;
            FramebufferHandle present_fb     = INVALID_HANDLE;

            ~DemoResources()
            {
                device.wait_idle();
                clear_present();
                device.destroy_framebuffer(offscreen_fb);
                device.destroy_render_pass(offscreen_pass);
                device.destroy_texture(image);
                device.destroy_buffer(readback);
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
                readback =
                    device.create_buffer({kBytes, BufferUsage::Transfer, MemoryUsage::GpuToCpu, "barrier readback"});
                offscreen_pass = make_color_pass(device, texture.format);
                if (image == INVALID_HANDLE || readback == INVALID_HANDLE || offscreen_pass == INVALID_HANDLE)
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
            const std::array<u8, 4> expected =
                verified_frames % 2 == 0 ? std::array<u8, 4>{32, 96, 192, 255} : std::array<u8, 4>{192, 64, 16, 255};
            ClearValue clear{};
            for (usize component = 0; component < expected.size(); ++component)
            {
                clear.color[component] = static_cast<f32>(expected[component]) / 255.0f;
            }
            FrameGraph graph;
            const auto image = graph.import_texture(
                "offscreen", resources.image, Format::R8G8B8A8_UNORM, kWidth, kHeight, resources.offscreen_fb);
            const auto readback = graph.import_buffer("readback", resources.readback);
            const auto screen   = graph.import_texture("screen",
                                                       device.get_swapchain_texture(),
                                                       device.get_swapchain_format(),
                                                       device.get_swapchain_width(),
                                                       device.get_swapchain_height(),
                                                       resources.present_fb);
            graph.set_final_access(readback, ResourceAccess::HostRead);
            graph.set_final_access(image, ResourceAccess::ShaderRead);
            bool copied = false;
            // Deliberately register the consumer before its producer.
            graph.add_pass(
                "readback",
                [&](PassBuilder& b) {
                    b.read(image, ResourceAccess::TransferRead);
                    b.write(readback, ResourceAccess::TransferWrite);
                },
                [&](PassExecuteContext& ctx) {
                    copied = ctx.device.cmd_copy_texture_to_buffer(ctx.cmd, resources.image, resources.readback);
                });
            const auto producer = graph.add_pass("offscreen clear", {}, {});
            graph.set_color_attachment(producer, image, clear);
            const auto present = graph.add_pass("screen clear", {}, {});
            graph.set_color_attachment(present, screen, clear);
            const bool recorded = graph.compile() && graph.execute(device);
            device.end_frame();
            device.present();
            device.wait_idle();
            if (!recorded || !copied)
            {
                ROVER_LOG_ERROR("Frame Graph demo: failed to record the readback graph");
                return 1;
            }
            const auto* pixels = static_cast<const u8*>(device.map_buffer(resources.readback));
            if (pixels == nullptr)
            {
                ROVER_LOG_ERROR("Frame Graph demo: could not map the readback buffer");
                return 1;
            }
            bool valid = true;
            for (usize byte = 0; byte < kBytes; ++byte)
            {
                if (pixels[byte] != expected[byte % 4])
                {
                    ROVER_LOG_ERROR("Frame Graph demo: frame {} byte {} expected {}, got {}",
                                    verified_frames,
                                    byte,
                                    expected[byte % 4],
                                    pixels[byte]);
                    valid = false;
                    break;
                }
            }
            device.unmap_buffer(resources.readback);
            if (!valid)
            {
                return 1;
            }
            ++verified_frames;
        }
        if (verified_frames != target_frames)
        {
            ROVER_LOG_ERROR("Frame Graph demo: window closed after only {} verified frames", verified_frames);
            return 1;
        }
        ROVER_LOG_INFO("Frame Graph GPU readback verified: {} frames, {} pixels per frame, alternating RGBA8 clears",
                       verified_frames,
                       kWidth * kHeight);
        return 0;
    }
} // namespace rover
