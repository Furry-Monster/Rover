#include "main/mesh_demo.h"

#include "core/log/log.h"
#include "core/math/mat4.h"
#include "core/math/quat.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "modules/scene/components/camera_component.h"
#include "modules/scene/components/light_component.h"
#include "modules/scene/components/mesh_component.h"
#include "modules/scene/components/name_component.h"
#include "modules/scene/components/transform_component.h"
#include "modules/scene/scene_tree.h"
#include "modules/scene/world.h"
#include "modules/serialization/mesh_asset_cache.h"
#include "modules/serialization/mesh_data.h"
#include "modules/serialization/scene_serializer.h"
#include "modules/serialization/texture_data.h"
#include "modules/serialization/texture_uploader.h"
#include "platform/linux/event_pump.h"
#include "platform/linux/time_source.h"
#include "platform/linux/window.h"
#include "services/graphics/frame_graph.h"
#include "services/graphics/graphics_service.h"
#include "services/graphics/material.h"
#include "shaders/mesh_frag.spv.h"
#include "shaders/mesh_vert.spv.h"

#include <cstring>
#include <unordered_map>
#include <vector>

namespace rover
{

    namespace
    {

        // ---------------------------------------------------------------------------
        // CPU-side push constant matching the layout in mesh.vert.glsl. Keep the
        // alignment / order in sync with the shader.
        // ---------------------------------------------------------------------------
        struct MeshPushConstants
        {
            Mat4    world;
            Vector4 light_dir;
            Vector4 light_color;
            Vector4 albedo;
        };

        static_assert(sizeof(MeshPushConstants) == 64 + 16 * 3, "MeshPushConstants must be 112 bytes");

        // CPU-side camera UBO matching mesh.vert.glsl set 0 binding 0.
        struct CameraUbo
        {
            Mat4 view;
            Mat4 projection;
        };

        // Demo-owned GPU resources. Tracked so we can wind them down cleanly on
        // shutdown (no asset registry lifetime management yet).
        struct DemoResources
        {
            RenderPassHandle                                     render_pass     = INVALID_HANDLE;
            ShaderHandle                                         mesh_vs         = INVALID_HANDLE;
            ShaderHandle                                         mesh_fs         = INVALID_HANDLE;
            BindGroupLayoutHandle                                bgl             = INVALID_HANDLE;
            PipelineLayoutHandle                                 pipeline_layout = INVALID_HANDLE;
            PipelineHandle                                       pipeline        = INVALID_HANDLE;
            BufferHandle                                         camera_ubo      = INVALID_HANDLE;
            BindGroupHandle                                      camera_bg       = INVALID_HANDLE;
            TextureGpuHandles                                    albedo_texture;
            TextureHandle                                        depth_texture = INVALID_HANDLE;
            std::unordered_map<TextureHandle, FramebufferHandle> framebuffers;
            u32                                                  fb_width  = 0;
            u32                                                  fb_height = 0;
        };

        bool create_static_resources(GraphicsDevice& device, DemoResources& d)
        {
            // ---- Render pass: swapchain color + depth ----
            RenderPassDesc      rp_desc{};
            ColorAttachmentDesc color{};
            color.format       = device.get_swapchain_format();
            color.load_op      = LoadOp::Clear;
            color.store_op     = StoreOp::Store;
            color.blend_enable = false;
            rp_desc.color_attachments.push_back(color);
            DepthStencilAttachmentDesc ds{};
            ds.format                 = Format::D32_SFLOAT;
            ds.load_op                = LoadOp::Clear;
            ds.store_op               = StoreOp::DontCare;
            rp_desc.depth_stencil     = ds;
            rp_desc.has_depth_stencil = true;
            rp_desc.debug_name        = "MeshDemoRenderPass";
            d.render_pass             = device.create_render_pass(rp_desc);
            if (d.render_pass == INVALID_HANDLE)
            {
                return false;
            }

            // ---- Shaders ----
            ShaderDesc vs{};
            vs.stage         = ShaderStage::Vertex;
            vs.bytecode      = reinterpret_cast<const u8*>(mesh_vert_spv);
            vs.bytecode_size = sizeof(mesh_vert_spv);
            d.mesh_vs        = device.create_shader(vs);

            ShaderDesc fs{};
            fs.stage         = ShaderStage::Fragment;
            fs.bytecode      = reinterpret_cast<const u8*>(mesh_frag_spv);
            fs.bytecode_size = sizeof(mesh_frag_spv);
            d.mesh_fs        = device.create_shader(fs);
            if (d.mesh_vs == INVALID_HANDLE || d.mesh_fs == INVALID_HANDLE)
            {
                return false;
            }

            // ---- Bind group layout (set 0: CameraUbo) ----
            BindGroupLayoutDesc  bgl_desc{};
            BindGroupLayoutEntry e{};
            e.binding          = 0;
            e.type             = BindingType::UniformBuffer;
            e.stage_visibility = ShaderStage::Vertex;
            bgl_desc.entries.push_back(e);
            bgl_desc.debug_name = "MeshCameraLayout";
            d.bgl               = device.create_bind_group_layout(bgl_desc);
            if (d.bgl == INVALID_HANDLE)
            {
                return false;
            }

            // ---- Pipeline layout (1 bind group + push constants) ----
            PipelineLayoutDesc pl_desc{};
            pl_desc.bind_group_layouts.push_back(d.bgl);
            PushConstantRange pc{};
            pc.stage_visibility = ShaderStage::Vertex | ShaderStage::Fragment;
            pc.offset           = 0;
            pc.size             = sizeof(MeshPushConstants);
            pl_desc.push_constants.push_back(pc);
            pl_desc.debug_name = "MeshDemoPipelineLayout";
            d.pipeline_layout  = device.create_pipeline_layout(pl_desc);
            if (d.pipeline_layout == INVALID_HANDLE)
            {
                return false;
            }

            // ---- Graphics pipeline (vertex layout matches MeshVertex) ----
            GraphicsPipelineDesc gp{};
            gp.vertex_shader   = d.mesh_vs;
            gp.fragment_shader = d.mesh_fs;
            gp.render_pass     = d.render_pass;
            gp.pipeline_layout = d.pipeline_layout;
            gp.topology        = PrimitiveTopology::TriangleList;
            gp.cull_mode       = CullMode::Back;
            // Matches Mat4::perspective canonical clip (positive viewport height, CCW front).
            gp.front_face         = FrontFace::CounterClockwise;
            gp.depth_test_enable  = true;
            gp.depth_write_enable = true;
            gp.depth_compare_op   = CompareOp::Less;
            gp.blend_enable       = false;

            VertexBinding vb{};
            vb.binding = 0;
            vb.stride  = sizeof(MeshVertex);
            vb.attributes.push_back({0, Format::R32G32B32_SFLOAT, 0});
            vb.attributes.push_back({1, Format::R32G32B32_SFLOAT, 12});
            vb.attributes.push_back({2, Format::R32G32_SFLOAT, 24});
            gp.vertex_bindings.push_back(vb);
            gp.debug_name = "MeshDemoPipeline";
            d.pipeline    = device.create_graphics_pipeline(gp);
            if (d.pipeline == INVALID_HANDLE)
            {
                return false;
            }

            // ---- Camera UBO + bind group ----
            BufferDesc ubo{};
            ubo.size       = sizeof(CameraUbo);
            ubo.usage      = BufferUsage::Uniform;
            ubo.memory     = MemoryUsage::CpuToGpu;
            ubo.debug_name = "CameraUbo";
            d.camera_ubo   = device.create_buffer(ubo);
            if (d.camera_ubo == INVALID_HANDLE)
            {
                return false;
            }

            BindGroupDesc bg_desc{};
            bg_desc.layout = d.bgl;
            BindGroupEntry be{};
            be.binding       = 0;
            be.type          = BindingType::UniformBuffer;
            be.buffer        = d.camera_ubo;
            be.buffer_offset = 0;
            be.buffer_size   = sizeof(CameraUbo);
            bg_desc.entries.push_back(be);
            bg_desc.debug_name = "CameraBindGroup";
            d.camera_bg        = device.create_bind_group(bg_desc);
            if (d.camera_bg == INVALID_HANDLE)
            {
                return false;
            }

            // ---- Procedural texture (exercises update_texture path even though
            //      this demo's shader doesn't sample it). Built unconditionally so
            //      the GPU upload path is verified end-to-end. ----
            auto checker     = make_checkerboard(64, 64, 8, 220, 220, 220, 80, 80, 80);
            d.albedo_texture = TextureUploader::upload(device, checker, "demo_checker");

            return true;
        }

        void rebuild_framebuffers(GraphicsDevice& device, DemoResources& d)
        {
            for (auto& [tex, fb] : d.framebuffers)
            {
                device.destroy_framebuffer(fb);
            }
            d.framebuffers.clear();

            if (d.depth_texture != INVALID_HANDLE)
            {
                device.destroy_texture(d.depth_texture);
                d.depth_texture = INVALID_HANDLE;
            }

            d.fb_width  = device.get_swapchain_width();
            d.fb_height = device.get_swapchain_height();

            if (d.fb_width > 0 && d.fb_height > 0)
            {
                TextureDesc depth_desc{};
                depth_desc.width      = d.fb_width;
                depth_desc.height     = d.fb_height;
                depth_desc.format     = Format::D32_SFLOAT;
                depth_desc.usage      = TextureUsage::DepthStencilAttachment;
                depth_desc.debug_name = "MeshDemoDepth";
                d.depth_texture       = device.create_texture(depth_desc);
                if (d.depth_texture == INVALID_HANDLE)
                {
                    ROVER_LOG_ERROR("Mesh demo: failed to create depth texture");
                }
            }

            const u32 image_count = device.get_swapchain_image_count();
            for (u32 i = 0; i < image_count; ++i)
            {
                const TextureHandle tex = device.get_swapchain_texture_at(i);
                if (tex == INVALID_HANDLE)
                {
                    continue;
                }
                FramebufferDesc fb_desc{};
                fb_desc.render_pass = d.render_pass;
                fb_desc.color_attachments.push_back(tex);
                fb_desc.depth_stencil      = d.depth_texture;
                fb_desc.width              = d.fb_width;
                fb_desc.height             = d.fb_height;
                const FramebufferHandle fb = device.create_framebuffer(fb_desc);
                if (fb != INVALID_HANDLE)
                {
                    d.framebuffers.emplace(tex, fb);
                }
            }
        }

        void destroy_demo(GraphicsDevice& device, DemoResources& d)
        {
            for (auto& [tex, fb] : d.framebuffers)
            {
                device.destroy_framebuffer(fb);
            }
            d.framebuffers.clear();

            if (d.depth_texture != INVALID_HANDLE)
            {
                device.destroy_texture(d.depth_texture);
                d.depth_texture = INVALID_HANDLE;
            }

            TextureUploader::destroy(device, d.albedo_texture);
            if (d.camera_bg != INVALID_HANDLE)
            {
                device.destroy_bind_group(d.camera_bg);
            }
            if (d.camera_ubo != INVALID_HANDLE)
            {
                device.destroy_buffer(d.camera_ubo);
            }
            if (d.pipeline != INVALID_HANDLE)
            {
                device.destroy_pipeline(d.pipeline);
            }
            if (d.pipeline_layout != INVALID_HANDLE)
            {
                device.destroy_pipeline_layout(d.pipeline_layout);
            }
            if (d.bgl != INVALID_HANDLE)
            {
                device.destroy_bind_group_layout(d.bgl);
            }
            if (d.mesh_fs != INVALID_HANDLE)
            {
                device.destroy_shader(d.mesh_fs);
            }
            if (d.mesh_vs != INVALID_HANDLE)
            {
                device.destroy_shader(d.mesh_vs);
            }
            if (d.render_pass != INVALID_HANDLE)
            {
                device.destroy_render_pass(d.render_pass);
            }
        }

        // Build a default Phase 2 scene: 1 camera + 1 directional light + 1 cube.
        void build_default_scene(World& world)
        {
            world.clear();

            auto camera = world.create_entity();
            world.add_component<NameComponent>(camera, "MainCamera");
            auto& cam_x         = world.add_component<TransformComponent>(camera);
            cam_x.position      = Vector3{0.0f, 0.0f, 4.0f};
            auto& cam_c         = world.add_component<CameraComponent>(camera);
            cam_c.fov_y_radians = 1.0472f;
            cam_c.near_plane    = 0.1f;
            cam_c.far_plane     = 100.0f;
            cam_c.primary       = true;

            auto light = world.create_entity();
            world.add_component<NameComponent>(light, "Sun");
            auto& l     = world.add_component<LightComponent>(light);
            l.type      = LightType::Directional;
            l.color     = Vector3{1.0f, 0.95f, 0.9f};
            l.intensity = 1.0f;
            l.direction = Vector3{-0.4f, -1.0f, -0.3f}.normalized();

            auto mesh_e = world.create_entity();
            world.add_component<NameComponent>(mesh_e, "Cube");
            world.add_component<TransformComponent>(mesh_e);
            world.add_component<MeshComponent>(mesh_e).mesh_asset = "builtin://meshes/cube";
        }

    } // namespace

    int run_mesh_demo(GraphicsDevice&    device,
                      Window&            window,
                      EventPump&         pump,
                      TimeSource&        time,
                      const std::string& scene_path,
                      u32                frame_limit)
    {
        DemoResources demo{};
        if (!create_static_resources(device, demo))
        {
            ROVER_LOG_ERROR("Failed to create mesh demo static resources");
            destroy_demo(device, demo);
            return 1;
        }
        rebuild_framebuffers(device, demo);

        auto directory = DirAccess::create();
        if (!directory)
        {
            ROVER_LOG_ERROR("Mesh demo requires a registered filesystem");
            destroy_demo(device, demo);
            return 1;
        }
        if (!directory->file_exists(scene_path))
        {
            World initial;
            build_default_scene(initial);
            if (!SceneSerializer::save_json(initial, scene_path))
            {
                ROVER_LOG_ERROR("Failed to create demo scene '{}'", scene_path);
                destroy_demo(device, demo);
                return 1;
            }
            ROVER_LOG_INFO("Created demo scene '{}'", scene_path);
        }

        World          world;
        MeshAssetCache meshes(device);
        if (!SceneSerializer::load_json(scene_path, world) || !meshes.resolve(world))
        {
            ROVER_LOG_ERROR("Failed to load scene '{}'", scene_path);
            destroy_demo(device, demo);
            return 1;
        }
        ROVER_LOG_INFO("Loaded scene '{}' ({} entities)", scene_path, world.entity_count());

        // ---- Bind device into GraphicsService for the frame loop ----
        auto& service = GraphicsService::get();
        service.bind_device(&device);

        ROVER_LOG_INFO("Mesh demo ready; entering main loop");

        u32 rendered_frames = 0;
        while (!window.should_close() && (frame_limit == 0 || rendered_frames < frame_limit))
        {
            pump.poll();
            time.tick();

            if (!device.begin_frame())
            {
                device.wait_idle();
                rebuild_framebuffers(device, demo);
                continue;
            }
            if (device.get_swapchain_width() != demo.fb_width || device.get_swapchain_height() != demo.fb_height)
            {
                device.wait_idle();
                rebuild_framebuffers(device, demo);
            }

            // ---- Update camera UBO ----
            CameraUbo ubo{};
            // Find primary camera entity.
            Mat4 view_mat = Mat4::identity();
            f32  aspect = static_cast<f32>(demo.fb_width) / static_cast<f32>(demo.fb_height == 0 ? 1 : demo.fb_height);
            Mat4 proj_mat  = Mat4::perspective(1.0472f, aspect, 0.1f, 100.0f);
            bool found_cam = false;
            world.each<CameraComponent, TransformComponent>([&](auto entity, CameraComponent& cc, TransformComponent&) {
                if (!cc.primary || found_cam)
                {
                    return;
                }
                found_cam       = true;
                cc.aspect_ratio = aspect;
                proj_mat        = cc.projection_matrix();
                view_mat        = SceneTree::world_matrix(world, entity).inverse();
            });
            ubo.view       = view_mat;
            ubo.projection = proj_mat;
            device.update_buffer(demo.camera_ubo, &ubo, sizeof(ubo));

            // ---- Find primary directional light ----
            Vector3 light_dir{0.0f, -1.0f, 0.0f};
            Vector3 light_color{1.0f, 1.0f, 1.0f};
            f32     light_intensity = 1.0f;
            world.each<LightComponent>([&](auto, LightComponent& lc) {
                if (lc.type != LightType::Directional)
                {
                    return;
                }
                light_dir       = lc.direction;
                light_color     = lc.color;
                light_intensity = lc.intensity;
            });

            // ---- Frame graph: forward pass writing to the swapchain ----
            auto& fg = service.frame_graph();
            fg.reset();
            const TextureHandle current_tex = device.get_swapchain_texture();
            const auto          fb_it       = demo.framebuffers.find(current_tex);
            if (fb_it == demo.framebuffers.end())
            {
                ROVER_LOG_WARN("No framebuffer for current swapchain texture");
                device.end_frame();
                device.present();
                continue;
            }

            auto color_id = fg.import_texture(
                "color", current_tex, device.get_swapchain_format(), demo.fb_width, demo.fb_height, fb_it->second);
            auto depth_id =
                fg.import_texture("depth", demo.depth_texture, Format::D32_SFLOAT, demo.fb_width, demo.fb_height);
            ClearValue clear{};
            clear.color[0] = 0.05f;
            clear.color[1] = 0.07f;
            clear.color[2] = 0.10f;
            clear.color[3] = 1.0f;
            clear.depth    = 1.0f;

            const u32 forward_idx = fg.add_pass(
                "forward",
                [&](PassBuilder& b) {
                    b.write(color_id);
                    b.write(depth_id, ResourceAccess::DepthStencilAttachment);
                },
                [&](PassExecuteContext& ctx) {
                    ctx.device.cmd_bind_pipeline(ctx.cmd, demo.pipeline);
                    Viewport vp{};
                    vp.x      = 0.0f;
                    vp.y      = 0.0f;
                    vp.width  = static_cast<f32>(demo.fb_width);
                    vp.height = static_cast<f32>(demo.fb_height);
                    ctx.device.cmd_set_viewport(ctx.cmd, vp);
                    Scissor sc{};
                    sc.width  = demo.fb_width;
                    sc.height = demo.fb_height;
                    ctx.device.cmd_set_scissor(ctx.cmd, sc);
                    ctx.device.cmd_bind_group(ctx.cmd, 0, demo.camera_bg);

                    world.each<TransformComponent, MeshComponent>(
                        [&](auto entity, TransformComponent&, MeshComponent& mc) {
                            if (mc.vertex_buffer == INVALID_HANDLE)
                            {
                                return;
                            }
                            MeshPushConstants pcdata{};
                            pcdata.world       = SceneTree::world_matrix(world, entity);
                            pcdata.light_dir   = Vector4{light_dir.x(), light_dir.y(), light_dir.z(), 0.0f};
                            pcdata.light_color = Vector4{light_color.x() * light_intensity,
                                                         light_color.y() * light_intensity,
                                                         light_color.z() * light_intensity,
                                                         0.15f};
                            pcdata.albedo      = Vector4{0.85f, 0.65f, 0.4f, 1.0f};
                            ctx.device.cmd_push_constants(ctx.cmd,
                                                          demo.pipeline_layout,
                                                          ShaderStage::Vertex | ShaderStage::Fragment,
                                                          &pcdata,
                                                          sizeof(pcdata));
                            ctx.device.cmd_bind_vertex_buffer(ctx.cmd, mc.vertex_buffer, 0, 0);
                            if (mc.index_buffer != INVALID_HANDLE)
                            {
                                ctx.device.cmd_bind_index_buffer(ctx.cmd, mc.index_buffer, mc.index_type, 0);
                                ctx.device.cmd_draw_indexed(ctx.cmd, mc.index_count, 1, 0, 0, 0);
                            }
                            else
                            {
                                ctx.device.cmd_draw(ctx.cmd, mc.vertex_count, 1, 0, 0);
                            }
                        });
                });
            fg.set_color_attachment(forward_idx, color_id, clear, true);

            if (!fg.compile())
            {
                ROVER_LOG_WARN("FrameGraph compile failed");
                device.end_frame();
                device.present();
                continue;
            }
            fg.execute(device);

            device.end_frame();
            device.present();
            ++rendered_frames;
        }

        ROVER_LOG_INFO("Rendered {} frames from '{}'; tearing down mesh demo resources", rendered_frames, scene_path);
        device.wait_idle();
        destroy_demo(device, demo);
        meshes.clear();
        service.unbind_device();
        return 0;
    }

} // namespace rover
