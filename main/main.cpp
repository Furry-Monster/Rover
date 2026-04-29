// Rover engine entry point.
//
// Initialization order follows the layer dependency direction:
//   core -> services -> drivers -> platform -> modules -> editor
// Shutdown is reverse order.
//
// Phase 2 milestone: render an ECS-driven cube + directional light using the
// FrameGraph + GraphicsService stack. The legacy Phase 1 triangle demo is
// retained behind `--demo triangle` for regression checks.

#include "core/register_core_types.h"
#include "drivers/register_driver_types.h"
#include "modules/register_module_types.h"
#include "platform/register_platform_apis.h"
#include "rover_version.h"
#include "services/register_service_types.h"

#ifdef ROVER_EDITOR_BUILD
    #include "editor/register_editor_types.h"
#endif

#include "core/graphics/graphics.h"
#include "core/log/log.h"
#include "drivers/vulkan/register_types.h"
#include "main/mesh_demo.h"
#include "platform/linux/event_pump.h"
#include "platform/linux/linux_platform.h"
#include "platform/linux/time_source.h"
#include "platform/linux/window.h"
#include "services/graphics/graphics_service.h"
#include "shaders/triangle_frag.spv.h"
#include "shaders/triangle_vert.spv.h"

#include <cstring>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace rover
{

    // ---------------------------------------------------------------------------
    // Phase 1 triangle demo -- preserved as a fallback path for `--demo triangle`.
    // ---------------------------------------------------------------------------
    struct TriangleDemo
    {
        RenderPassHandle                                     render_pass     = INVALID_HANDLE;
        ShaderHandle                                         vertex_shader   = INVALID_HANDLE;
        ShaderHandle                                         fragment_shader = INVALID_HANDLE;
        PipelineHandle                                       pipeline        = INVALID_HANDLE;
        std::unordered_map<TextureHandle, FramebufferHandle> framebuffers;
        u32                                                  last_swapchain_width  = 0;
        u32                                                  last_swapchain_height = 0;
        Format                                               last_swapchain_format = Format::UNDEFINED;
    };

    static bool create_triangle_resources(GraphicsDevice& device, TriangleDemo& demo)
    {
        demo.last_swapchain_format = device.get_swapchain_format();

        RenderPassDesc      rp_desc{};
        ColorAttachmentDesc color{};
        color.format       = demo.last_swapchain_format;
        color.load_op      = LoadOp::Clear;
        color.store_op     = StoreOp::Store;
        color.blend_enable = false;
        rp_desc.color_attachments.push_back(color);
        rp_desc.has_depth_stencil = false;
        rp_desc.debug_name        = "TriangleRenderPass";
        demo.render_pass          = device.create_render_pass(rp_desc);
        if (demo.render_pass == INVALID_HANDLE)
        {
            return false;
        }

        ShaderDesc vs_desc{};
        vs_desc.stage         = ShaderStage::Vertex;
        vs_desc.bytecode      = reinterpret_cast<const u8*>(triangle_vert_spv);
        vs_desc.bytecode_size = sizeof(triangle_vert_spv);
        vs_desc.entry_point   = "main";
        demo.vertex_shader    = device.create_shader(vs_desc);

        ShaderDesc fs_desc{};
        fs_desc.stage         = ShaderStage::Fragment;
        fs_desc.bytecode      = reinterpret_cast<const u8*>(triangle_frag_spv);
        fs_desc.bytecode_size = sizeof(triangle_frag_spv);
        fs_desc.entry_point   = "main";
        demo.fragment_shader  = device.create_shader(fs_desc);
        if (demo.vertex_shader == INVALID_HANDLE || demo.fragment_shader == INVALID_HANDLE)
        {
            return false;
        }

        GraphicsPipelineDesc pipe_desc{};
        pipe_desc.vertex_shader      = demo.vertex_shader;
        pipe_desc.fragment_shader    = demo.fragment_shader;
        pipe_desc.render_pass        = demo.render_pass;
        pipe_desc.topology           = PrimitiveTopology::TriangleList;
        pipe_desc.cull_mode          = CullMode::None;
        pipe_desc.front_face         = FrontFace::CounterClockwise;
        pipe_desc.depth_test_enable  = false;
        pipe_desc.depth_write_enable = false;
        pipe_desc.blend_enable       = false;
        pipe_desc.debug_name         = "TrianglePipeline";
        demo.pipeline                = device.create_graphics_pipeline(pipe_desc);
        return demo.pipeline != INVALID_HANDLE;
    }

    static void destroy_triangle_resources(GraphicsDevice& device, TriangleDemo& demo)
    {
        if (demo.pipeline != INVALID_HANDLE)
        {
            device.destroy_pipeline(demo.pipeline);
        }
        if (demo.fragment_shader != INVALID_HANDLE)
        {
            device.destroy_shader(demo.fragment_shader);
        }
        if (demo.vertex_shader != INVALID_HANDLE)
        {
            device.destroy_shader(demo.vertex_shader);
        }
        if (demo.render_pass != INVALID_HANDLE)
        {
            device.destroy_render_pass(demo.render_pass);
        }
        for (auto& [tex, fb] : demo.framebuffers)
        {
            device.destroy_framebuffer(fb);
        }
        demo.framebuffers.clear();
    }

    static void rebuild_triangle_framebuffers(GraphicsDevice& device, TriangleDemo& demo)
    {
        for (auto& [tex, fb] : demo.framebuffers)
        {
            device.destroy_framebuffer(fb);
        }
        demo.framebuffers.clear();
        demo.last_swapchain_width  = device.get_swapchain_width();
        demo.last_swapchain_height = device.get_swapchain_height();
        const u32 image_count      = device.get_swapchain_image_count();
        for (u32 i = 0; i < image_count; ++i)
        {
            const TextureHandle tex = device.get_swapchain_texture_at(i);
            if (tex == INVALID_HANDLE)
            {
                continue;
            }
            FramebufferDesc fb_desc{};
            fb_desc.render_pass = demo.render_pass;
            fb_desc.color_attachments.push_back(tex);
            fb_desc.width              = demo.last_swapchain_width;
            fb_desc.height             = demo.last_swapchain_height;
            const FramebufferHandle fb = device.create_framebuffer(fb_desc);
            if (fb != INVALID_HANDLE)
            {
                demo.framebuffers.emplace(tex, fb);
            }
        }
    }

    static int run_triangle_demo(GraphicsDevice& device, Window& window, EventPump& pump, TimeSource& time)
    {
        TriangleDemo demo{};
        if (!create_triangle_resources(device, demo))
        {
            destroy_triangle_resources(device, demo);
            return 1;
        }
        rebuild_triangle_framebuffers(device, demo);
        ROVER_LOG_INFO("Triangle demo ready; entering main loop");

        while (!window.should_close())
        {
            pump.poll();
            time.tick();

            if (!device.begin_frame())
            {
                device.wait_idle();
                rebuild_triangle_framebuffers(device, demo);
                continue;
            }

            if (device.get_swapchain_width() != demo.last_swapchain_width ||
                device.get_swapchain_height() != demo.last_swapchain_height)
            {
                device.wait_idle();
                rebuild_triangle_framebuffers(device, demo);
            }

            const TextureHandle current_tex = device.get_swapchain_texture();
            const auto          fb_it       = demo.framebuffers.find(current_tex);
            if (fb_it == demo.framebuffers.end())
            {
                device.end_frame();
                device.present();
                continue;
            }

            const CommandListHandle cmd = device.begin_command_list();
            ClearValue              clear{};
            clear.color[0] = 0.05f;
            clear.color[1] = 0.05f;
            clear.color[2] = 0.10f;
            clear.color[3] = 1.0f;
            device.cmd_begin_render_pass(cmd, fb_it->second, &clear, 1);
            device.cmd_bind_pipeline(cmd, demo.pipeline);
            Viewport vp{};
            vp.width  = static_cast<f32>(demo.last_swapchain_width);
            vp.height = static_cast<f32>(demo.last_swapchain_height);
            device.cmd_set_viewport(cmd, vp);
            Scissor sc{};
            sc.width  = demo.last_swapchain_width;
            sc.height = demo.last_swapchain_height;
            device.cmd_set_scissor(cmd, sc);
            device.cmd_draw(cmd, 3, 1, 0, 0);
            device.cmd_end_render_pass(cmd);
            device.end_command_list(cmd);
            device.end_frame();
            device.present();
        }

        device.wait_idle();
        destroy_triangle_resources(device, demo);
        return 0;
    }

    // ---------------------------------------------------------------------------
    // Top-level dispatcher: parses --demo and routes to the chosen demo.
    // ---------------------------------------------------------------------------
    static int run_main_loop(std::string_view demo_name)
    {
        auto& platform = LinuxPlatform::get();
        if (!platform.initialized())
        {
            ROVER_LOG_ERROR("Linux platform not initialized; aborting main loop");
            return 1;
        }

        auto* device = get_vulkan_device();
        if (device == nullptr)
        {
            ROVER_LOG_ERROR("Vulkan driver not registered; aborting main loop");
            return 1;
        }

        Window&     window = platform.window();
        EventPump&  pump   = platform.event_pump();
        TimeSource& time   = platform.time();

        if (!device->init(window))
        {
            ROVER_LOG_ERROR("Failed to initialize Vulkan device");
            return 1;
        }
        ROVER_LOG_INFO("GPU: {} ({})", device->get_device_name(), device->get_api_name());
        ROVER_LOG_INFO("Swapchain: {}x{}, format {}",
                       device->get_swapchain_width(),
                       device->get_swapchain_height(),
                       static_cast<i32>(device->get_swapchain_format()));

        int rc = 0;
        if (demo_name == "triangle")
        {
            rc = run_triangle_demo(*device, window, pump, time);
        }
        else
        {
            rc = run_mesh_demo(*device, window, pump, time);
        }
        device->shutdown();
        return rc;
    }

    static int rover_main(int argc, char** argv)
    {
        register_core_types();

        ROVER_LOG_INFO("{} {}", ROVER_VERSION_NAME, ROVER_VERSION_STRING);

        register_service_types();
        register_driver_types();
        register_platform_apis();
        register_module_types();
#ifdef ROVER_EDITOR_BUILD
        register_editor_types();
#endif

        // Argument parsing: only --demo <triangle|mesh> for now.
        std::string_view demo_name = "mesh";
        for (int i = 1; i + 1 < argc; ++i)
        {
            if (std::strcmp(argv[i], "--demo") == 0)
            {
                demo_name = argv[i + 1];
            }
        }

        const int exit_code = run_main_loop(demo_name);

#ifdef ROVER_EDITOR_BUILD
        unregister_editor_types();
#endif
        unregister_module_types();
        unregister_platform_apis();
        unregister_driver_types();
        unregister_service_types();
        unregister_core_types();
        return exit_code;
    }

} // namespace rover

int
main(int argc, char** argv)
{
    return rover::rover_main(argc, argv);
}
