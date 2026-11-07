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
#include "platform/linux/event_pump.h"
#include "platform/linux/time_source.h"
#include "platform/linux/window.h"
#include "services/graphics/frame_graph.h"
#include "services/graphics/graphics_service.h"
#include "services/graphics/post_process_pipeline.h"
#include "shaders/mesh_frag.spv.h"
#include "shaders/mesh_vert.spv.h"
#include "shaders/post_process_vert.spv.h"
#include "shaders/present_frag.spv.h"
#include "shaders/tone_map_frag.spv.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace rover
{

    namespace
    {

        // ---------------------------------------------------------------------------
        // CPU-side push constant matching the layout in mesh.slang. Keep the
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

        // CPU-side camera UBO matching mesh.slang set 0 binding 0.
        struct CameraUbo
        {
            Mat4 view;
            Mat4 projection;
        };

        struct DemoResources
        {
            explicit DemoResources(GraphicsDevice& graphics) : device(graphics), post(graphics) {}

            DemoResources(const DemoResources&)                   = delete;
            DemoResources&        operator=(const DemoResources&) = delete;
            GraphicsDevice&       device;
            PostProcessPipeline   post;
            ShaderHandle          mesh_vs         = INVALID_HANDLE;
            ShaderHandle          mesh_fs         = INVALID_HANDLE;
            BindGroupLayoutHandle bgl             = INVALID_HANDLE;
            PipelineLayoutHandle  pipeline_layout = INVALID_HANDLE;
            PipelineHandle        pipeline        = INVALID_HANDLE;

            struct CameraBinding
            {
                BufferHandle    buffer = INVALID_HANDLE;
                BindGroupHandle group  = INVALID_HANDLE;
            };

            std::unordered_map<TextureHandle, CameraBinding> cameras;
            u64                                              camera_generation = 0;

            void clear_cameras(std::unordered_map<TextureHandle, CameraBinding>& bindings)
            {
                for (const auto& [texture, camera] : bindings)
                {
                    if (camera.group)
                    {
                        device.destroy_bind_group(camera.group);
                    }
                    if (camera.buffer)
                    {
                        device.destroy_buffer(camera.buffer);
                    }
                }
                bindings.clear();
            }

            ~DemoResources()
            {
                device.wait_idle();
                clear_cameras(cameras);
                if (pipeline)
                {
                    device.destroy_pipeline(pipeline);
                }
                if (pipeline_layout)
                {
                    device.destroy_pipeline_layout(pipeline_layout);
                }
                if (bgl)
                {
                    device.destroy_bind_group_layout(bgl);
                }
                if (mesh_fs)
                {
                    device.destroy_shader(mesh_fs);
                }
                if (mesh_vs)
                {
                    device.destroy_shader(mesh_vs);
                }
                post.shutdown();
            }

            bool sync_cameras()
            {
                if (camera_generation == post.generation())
                {
                    return true;
                }
                std::unordered_map<TextureHandle, CameraBinding> next;
                for (u32 i = 0; i < device.get_swapchain_image_count(); ++i)
                {
                    auto& camera  = next[device.get_swapchain_texture_at(i)];
                    camera.buffer = device.create_buffer(
                        {sizeof(CameraUbo), BufferUsage::Uniform, MemoryUsage::CpuToGpu, "CameraUbo"});
                    if (!camera.buffer)
                    {
                        clear_cameras(next);
                        return false;
                    }
                    BindGroupDesc group{};
                    group.layout = bgl;
                    BindGroupEntry entry{};
                    entry.binding     = 0;
                    entry.type        = BindingType::UniformBuffer;
                    entry.buffer      = camera.buffer;
                    entry.buffer_size = sizeof(CameraUbo);
                    group.entries     = {entry};
                    camera.group      = device.create_bind_group(group);
                    if (!camera.group)
                    {
                        clear_cameras(next);
                        return false;
                    }
                }
                clear_cameras(cameras); // post.prepare() already waited for GPU idle.
                cameras           = std::move(next);
                camera_generation = post.generation();
                return true;
            }
        };

        template <usize N>
        ShaderDesc shader_desc(ShaderStage stage, const std::uint32_t (&code)[N])
        {
            return {stage, reinterpret_cast<const u8*>(code), sizeof(code), "main"};
        }

        bool create_static_resources(GraphicsDevice& device, DemoResources& d, const MeshDemoOptions& options)
        {
            if (!d.post.initialize(shader_desc(ShaderStage::Vertex, post_process_vert_spv),
                                   shader_desc(ShaderStage::Fragment, tone_map_frag_spv),
                                   shader_desc(ShaderStage::Fragment, present_frag_spv),
                                   options.verify_rendering || !options.capture_path.empty()))
            {
                return false;
            }
            d.mesh_vs = device.create_shader(shader_desc(ShaderStage::Vertex, mesh_vert_spv));
            d.mesh_fs = device.create_shader(shader_desc(ShaderStage::Fragment, mesh_frag_spv));
            BindGroupLayoutDesc layout{};
            layout.entries = {{0, BindingType::UniformBuffer, ShaderStage::Vertex}};
            d.bgl          = device.create_bind_group_layout(layout);
            if (!d.mesh_vs || !d.mesh_fs || !d.bgl)
            {
                return false;
            }
            PipelineLayoutDesc pipeline_layout{};
            pipeline_layout.bind_group_layouts = {d.bgl};
            pipeline_layout.push_constants     = {
                {ShaderStage::Vertex | ShaderStage::Fragment, 0, sizeof(MeshPushConstants)}};
            d.pipeline_layout = device.create_pipeline_layout(pipeline_layout);
            if (!d.pipeline_layout)
            {
                return false;
            }
            GraphicsPipelineDesc gp{};
            gp.vertex_shader      = d.mesh_vs;
            gp.fragment_shader    = d.mesh_fs;
            gp.render_pass        = d.post.scene_render_pass();
            gp.pipeline_layout    = d.pipeline_layout;
            gp.cull_mode          = CullMode::Back;
            gp.front_face         = FrontFace::CounterClockwise;
            gp.depth_test_enable  = true;
            gp.depth_write_enable = true;
            gp.depth_compare_op   = CompareOp::Less;
            VertexBinding vertex{};
            vertex.binding     = 0;
            vertex.stride      = sizeof(MeshVertex);
            vertex.attributes  = {{0, Format::R32G32B32_SFLOAT, 0}, {1, Format::R32G32B32_SFLOAT, 12}};
            gp.vertex_bindings = {vertex};
            d.pipeline         = device.create_graphics_pipeline(gp);
            return d.pipeline != INVALID_HANDLE;
        }

        bool verify_pixels(const std::vector<u8>& pixels, const MeshDemoOptions& options)
        {
            const f32 background[3] = {0.05f, 0.07f, 0.10f};
            int       expected[3]{};
            for (usize c = 0; c < 3; ++c)
            {
                const f32 exposed = background[c] * options.exposure;
                const f32 mapped  = options.tone_mapping ? exposed / (1.0f + exposed) : std::min(exposed, 1.0f);
                expected[c]       = static_cast<int>(std::lround(mapped * 255.0f));
            }
            usize background_pixels = 0;
            usize geometry_pixels   = 0;
            f64   sum               = 0.0;
            for (usize i = 0; i < pixels.size(); i += 4)
            {
                bool background_match = true;
                for (usize c = 0; c < 3; ++c)
                {
                    background_match &= std::abs(static_cast<int>(pixels[i + c]) - expected[c]) <= 2;
                    sum += pixels[i + c];
                }
                if (pixels[i + 3] != 255)
                {
                    ROVER_LOG_ERROR("Scene readback: invalid output alpha");
                    return false;
                }
                if (background_match)
                {
                    ++background_pixels;
                }
                else
                {
                    ++geometry_pixels;
                }
            }
            if (background_pixels == 0 || geometry_pixels == 0)
            {
                ROVER_LOG_ERROR("Scene readback requires visible geometry and background: background={}, geometry={}",
                                background_pixels,
                                geometry_pixels);
                return false;
            }
            ROVER_LOG_INFO(
                "Scene readback verified: background={}, geometry={}, mean_rgb={:.3f}, exposure={}, tone_map={}",
                background_pixels,
                geometry_pixels,
                sum / static_cast<f64>(pixels.size() / 4 * 3),
                options.exposure,
                options.tone_mapping);
            return true;
        }

        bool save_ppm(const std::string& path, u32 width, u32 height, const std::vector<u8>& pixels)
        {
            auto file = FileAccess::create();
            if (!file || !file->open(path, FileMode::Write))
            {
                return false;
            }
            const auto      header = "P6\n" + std::to_string(width) + " " + std::to_string(height) + "\n255\n";
            std::vector<u8> rgb;
            rgb.reserve(pixels.size() / 4 * 3);
            for (usize i = 0; i < pixels.size(); i += 4)
            {
                rgb.insert(rgb.end(), {pixels[i], pixels[i + 1], pixels[i + 2]});
            }
            if (!file->write_all_text(header) || file->write(rgb.data(), rgb.size()) != rgb.size())
            {
                return false;
            }
            ROVER_LOG_INFO("Captured post-process output to '{}' ({}x{})", path, width, height);
            return true;
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

    int run_mesh_demo(GraphicsDevice&        device,
                      Window&                window,
                      EventPump&             pump,
                      TimeSource&            time,
                      const std::string&     scene_path,
                      u32                    frame_limit,
                      const MeshDemoOptions& options)
    {
        DemoResources demo{device};
        if (!create_static_resources(device, demo, options))
        {
            ROVER_LOG_ERROR("Failed to create scene render resources");
            return 1;
        }
        auto directory = DirAccess::create();
        if (!directory)
        {
            ROVER_LOG_ERROR("Mesh demo requires a registered filesystem");
            return 1;
        }
        if (!directory->file_exists(scene_path))
        {
            World initial;
            build_default_scene(initial);
            if (!SceneSerializer::save_json(initial, scene_path))
            {
                return 1;
            }
        }
        World          world;
        MeshAssetCache meshes(device);
        if (!SceneSerializer::load_json(scene_path, world) || !meshes.resolve(world))
        {
            ROVER_LOG_ERROR("Failed to load scene '{}'", scene_path);
            return 1;
        }
        ROVER_LOG_INFO("Loaded scene '{}' ({} entities)", scene_path, world.entity_count());
        auto& service = GraphicsService::get();
        service.bind_device(&device);

        struct ServiceBinding
        {
            GraphicsService& service;

            ~ServiceBinding()
            {
                service.frame_graph().reset();
                service.unbind_device();
            }
        } binding{service};

        u32             rendered_frames = 0;
        u32             initial_width   = 0;
        u32             initial_height  = 0;
        std::vector<u8> last_pixels;
        while (!window.should_close() && (frame_limit == 0 || rendered_frames < frame_limit))
        {
            pump.poll();
            time.tick();
            if (!device.begin_frame())
            {
                continue;
            }
            if (!demo.post.prepare() || !demo.sync_cameras())
            {
                device.end_frame();
                device.present();
                ROVER_LOG_ERROR("Failed to prepare scene render targets or camera buffers");
                return 1;
            }
            const auto  screen = device.get_swapchain_texture();
            const auto& camera = demo.cameras.at(screen);
            const u32   width  = demo.post.width();
            const u32   height = demo.post.height();
            if (initial_width == 0)
            {
                initial_width  = width;
                initial_height = height;
            }
            const f32 aspect = static_cast<f32>(width) / static_cast<f32>(height);
            CameraUbo ubo{};
            ubo.view          = Mat4::identity();
            ubo.projection    = Mat4::perspective(1.0472f, aspect, 0.1f, 100.0f);
            bool found_camera = false;
            world.each<CameraComponent, TransformComponent>(
                [&](auto entity, CameraComponent& component, TransformComponent&) {
                    if (component.primary && !found_camera)
                    {
                        found_camera           = true;
                        component.aspect_ratio = aspect;
                        ubo.projection         = component.projection_matrix();
                        ubo.view               = SceneTree::world_matrix(world, entity).inverse();
                    }
                });
            device.update_buffer(camera.buffer, &ubo, sizeof(ubo));
            Vector3 light_dir{0.0f, -1.0f, 0.0f};
            Vector3 light_color{1.0f, 1.0f, 1.0f};
            f32     light_intensity = 1.0f;
            world.each<LightComponent>([&](auto, LightComponent& light) {
                if (light.type == LightType::Directional)
                {
                    light_dir       = light.direction;
                    light_color     = light.color;
                    light_intensity = light.intensity;
                }
            });
            auto& graph = service.frame_graph();
            graph.reset();
            const auto                                         camera_id = graph.import_buffer("camera", camera.buffer);
            std::unordered_map<BufferHandle, RenderResourceId> mesh_resources;
            const bool                                         added = demo.post.add_passes(
                graph,
                screen,
                [&](PassBuilder& builder) {
                    builder.read(camera_id, ResourceAccess::UniformRead);
                    auto read_mesh_buffer = [&](BufferHandle buffer, ResourceAccess access) {
                        if (!buffer)
                        {
                            return;
                        }
                        auto [it, inserted] = mesh_resources.try_emplace(buffer, INVALID_RESOURCE_ID);
                        if (inserted)
                        {
                            it->second = graph.import_buffer("mesh", buffer);
                        }
                        builder.read(it->second, access);
                    };
                    world.each<MeshComponent>([&](auto, MeshComponent& mesh) {
                        read_mesh_buffer(mesh.vertex_buffer, ResourceAccess::VertexRead);
                        read_mesh_buffer(mesh.index_buffer, ResourceAccess::IndexRead);
                    });
                },
                [&](PassExecuteContext& ctx) {
                    ctx.device.cmd_bind_pipeline(ctx.cmd, demo.pipeline);
                    ctx.device.cmd_set_viewport(ctx.cmd,
                                                {0.0f, 0.0f, static_cast<f32>(width), static_cast<f32>(height)});
                    ctx.device.cmd_set_scissor(ctx.cmd, {0, 0, width, height});
                    ctx.device.cmd_bind_group(ctx.cmd, 0, camera.group);
                    world.each<TransformComponent, MeshComponent>(
                        [&](auto entity, TransformComponent&, MeshComponent& mesh) {
                            if (!mesh.vertex_buffer)
                            {
                                return;
                            }
                            MeshPushConstants pc{};
                            pc.world       = SceneTree::world_matrix(world, entity);
                            pc.light_dir   = {light_dir.x(), light_dir.y(), light_dir.z(), 0.0f};
                            pc.light_color = {light_color.x() * light_intensity,
                                              light_color.y() * light_intensity,
                                              light_color.z() * light_intensity,
                                              0.15f};
                            pc.albedo      = {0.85f, 0.65f, 0.4f, 1.0f};
                            ctx.device.cmd_push_constants(ctx.cmd,
                                                          demo.pipeline_layout,
                                                          ShaderStage::Vertex | ShaderStage::Fragment,
                                                          &pc,
                                                          sizeof(pc));
                            ctx.device.cmd_bind_vertex_buffer(ctx.cmd, mesh.vertex_buffer, 0, 0);
                            if (mesh.index_buffer)
                            {
                                ctx.device.cmd_bind_index_buffer(ctx.cmd, mesh.index_buffer, mesh.index_type, 0);
                                ctx.device.cmd_draw_indexed(ctx.cmd, mesh.index_count, 1, 0, 0, 0);
                            }
                            else
                            {
                                ctx.device.cmd_draw(ctx.cmd, mesh.vertex_count, 1, 0, 0);
                            }
                        });
                },
                {options.exposure, options.tone_mapping});
            const bool recorded = added && graph.compile() && graph.execute(device);
            device.end_frame();
            device.present();
            if (!recorded)
            {
                return 1;
            }
            if (demo.post.readback_enabled())
            {
                if (!demo.post.read_pixels(screen, last_pixels))
                {
                    return 1;
                }
                if (options.verify_rendering && !verify_pixels(last_pixels, options))
                {
                    return 1;
                }
            }
            ++rendered_frames;
            if (options.resize_test && rendered_frames == 2)
            {
                const u32 next_width  = initial_width == 960 ? 1120 : 960;
                const u32 next_height = initial_height == 540 ? 630 : 540;
                if (!window.resize(next_width, next_height))
                {
                    return 1;
                }
            }
        }
        if (options.resize_test && (demo.post.generation() < 2 ||
                                    (demo.post.width() == initial_width && demo.post.height() == initial_height)))
        {
            ROVER_LOG_ERROR("Resize verification failed: no updated render target extent");
            return 1;
        }
        if (!options.capture_path.empty() &&
            (last_pixels.empty() ||
             !save_ppm(options.capture_path, demo.post.width(), demo.post.height(), last_pixels)))
        {
            ROVER_LOG_ERROR("Could not save capture '{}'", options.capture_path);
            return 1;
        }
        ROVER_LOG_INFO(
            "Rendered {} frames from '{}' through HDR, tone mapping and presentation", rendered_frames, scene_path);
        return 0;
    }

} // namespace rover
