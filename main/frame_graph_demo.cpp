#include "main/frame_graph_demo.h"

#include "core/graphics/graphics_device.h"
#include "core/log/log.h"
#include "platform/linux/event_pump.h"
#include "platform/linux/window.h"
#include "services/graphics/frame_graph.h"
#include "services/graphics/transient_resource_pool.h"

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
            explicit DemoResources(GraphicsDevice& graphics) : device(graphics), pool(graphics) {}

            GraphicsDevice&            device;
            TransientResourcePool      pool;
            RenderPassHandle           offscreen_pass = INVALID_HANDLE;
            RenderPassHandle           present_pass   = INVALID_HANDLE;
            Format                     format         = Format::UNDEFINED;
            u32                        width = 0, height = 0;
            std::vector<TextureHandle> screens;

            ~DemoResources()
            {
                pool.clear();
                device.destroy_render_pass(present_pass);
                device.destroy_render_pass(offscreen_pass);
            }

            bool init()
            {
                offscreen_pass = make_color_pass(device, Format::R8G8B8A8_UNORM);
                return offscreen_pass != INVALID_HANDLE;
            }

            bool prepare_present()
            {
                std::vector<TextureHandle> images;
                for (u32 i = 0; i < device.get_swapchain_image_count(); ++i)
                {
                    images.push_back(device.get_swapchain_texture_at(i));
                }
                if (present_pass && format == device.get_swapchain_format() && width == device.get_swapchain_width() &&
                    height == device.get_swapchain_height() && screens == images)
                {
                    return true;
                }
                if (!pool.clear())
                {
                    return false;
                }
                device.destroy_render_pass(present_pass);
                format       = device.get_swapchain_format();
                width        = device.get_swapchain_width();
                height       = device.get_swapchain_height();
                screens      = std::move(images);
                present_pass = make_color_pass(device, format);
                return present_pass != INVALID_HANDLE;
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
            const std::array<u8, 4>                green{64, 224, 96, 255};
            const std::array<std::array<u8, 4>, 3> expected =
                verified_frames % 2 == 0 ? std::array{blue, red, green} : std::array{red, blue, green};
            std::array<ClearValue, 3> clears{};
            for (usize version = 0; version < expected.size(); ++version)
            {
                for (usize component = 0; component < 4; ++component)
                {
                    clears[version].color[component] = static_cast<f32>(expected[version][component]) / 255.0f;
                }
            }
            FrameGraph  graph;
            TextureDesc desc{};
            desc.width             = kWidth;
            desc.height            = kHeight;
            desc.format            = Format::R8G8B8A8_UNORM;
            desc.usage             = TextureUsage::ColorAttachment | TextureUsage::TransferSrc | TextureUsage::Sampled;
            const auto image       = graph.create_texture("offscreen.v0", desc);
            const auto reused      = graph.create_texture("reused", desc);
            const auto overwritten = graph.create_version(image, "offscreen.v1");
            const auto screen      = graph.import_texture("screen",
                                                          device.get_swapchain_texture(),
                                                          device.get_swapchain_format(),
                                                          device.get_swapchain_width(),
                                                          device.get_swapchain_height());
            graph.set_final_access(reused, ResourceAccess::ShaderRead);
            std::array<bool, 3>             copied{};
            std::array<RenderResourceId, 3> readbacks{};
            auto                            add_readback = [&](usize version, RenderResourceId source) {
                const auto buffer  = graph.create_buffer("readback.v" + std::to_string(version),
                                                         {kBytes, BufferUsage::Transfer, MemoryUsage::GpuToCpu});
                readbacks[version] = buffer;
                graph.set_final_access(buffer, ResourceAccess::HostRead);
                graph.add_pass(
                    "readback.v" + std::to_string(version),
                    [=](PassBuilder& b) {
                        b.read(source, ResourceAccess::TransferRead);
                        b.write(buffer, ResourceAccess::TransferWrite);
                    },
                    [&, version, source, buffer](PassExecuteContext& ctx) {
                        copied[version] = ctx.device.cmd_copy_texture_to_buffer(
                            ctx.cmd, ctx.graph.resource(source).texture, ctx.graph.resource(buffer).buffer);
                    });
            };
            // Register the overwrite before the old reader: WAR must move that
            // reader ahead of the overwrite, while RAW finds both producers.
            add_readback(1, overwritten);
            const auto overwrite = graph.add_pass("overwrite.v1", {}, {});
            graph.set_render_target(overwrite, resources.offscreen_pass, overwritten, clears[1]);
            add_readback(0, image);
            const auto producer = graph.add_pass("clear.v0", {}, {});
            graph.set_render_target(producer, resources.offscreen_pass, image, clears[0]);
            const auto present = graph.add_pass("screen clear", {}, {});
            graph.set_render_target(present, resources.present_pass, screen, clears[1]);
            add_readback(2, reused);
            const auto reuse_writer = graph.add_pass("reuse physical texture", {}, {});
            graph.set_render_target(reuse_writer, resources.offscreen_pass, reused, clears[2]);
            const bool prepared = graph.compile() && graph.prepare_resources(resources.pool);
            const bool aliases  = prepared && graph.resource(image).texture == graph.resource(reused).texture;
            const bool recorded = aliases && graph.execute(device);
            device.end_frame();
            device.present();
            device.wait_idle();
            if (!recorded || !copied[0] || !copied[1] || !copied[2])
            {
                ROVER_LOG_ERROR("Frame Graph demo: failed to record the readback graph");
                return 1;
            }
            for (usize version = 0; version < expected.size(); ++version)
            {
                const auto* pixels =
                    static_cast<const u8*>(device.map_buffer(graph.resource(readbacks[version]).buffer));
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
                device.unmap_buffer(graph.resource(readbacks[version]).buffer);
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
        ROVER_LOG_INFO(
            "Frame Graph GPU readback verified: {} frames, 2 versions + pooled texture reuse, {} pixels per result",
            verified_frames,
            kWidth * kHeight);
        return 0;
    }
} // namespace rover
