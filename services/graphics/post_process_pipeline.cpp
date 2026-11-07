#include "services/graphics/post_process_pipeline.h"

#include "core/log/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace rover
{
    namespace
    {
        RenderPassHandle color_pass(GraphicsDevice& device, Format format, bool depth = false)
        {
            RenderPassDesc      desc{};
            ColorAttachmentDesc color{};
            color.format = format;
            desc.color_attachments.push_back(color);
            desc.has_depth_stencil      = depth;
            desc.depth_stencil.format   = Format::D32_SFLOAT;
            desc.depth_stencil.store_op = StoreOp::DontCare;
            return device.create_render_pass(desc);
        }

        template <typename Method>
        void release(GraphicsDevice& device, u64& handle, Method method)
        {
            if (handle != INVALID_HANDLE)
            {
                (device.*method)(handle);
                handle = INVALID_HANDLE;
            }
        }
    } // namespace

    PostProcessPipeline::~PostProcessPipeline()
    {
        shutdown();
    }

    bool PostProcessPipeline::initialize(const ShaderDesc& vertex,
                                         const ShaderDesc& tone_map,
                                         const ShaderDesc& present,
                                         bool              enable_readback)
    {
        if (initialized_)
        {
            ROVER_LOG_ERROR("PostProcessPipeline: already initialized");
            return false;
        }
        vertex_         = device_.create_shader(vertex);
        tone_shader_    = device_.create_shader(tone_map);
        present_shader_ = device_.create_shader(present);
        sampler_        = device_.create_sampler({Filter::Nearest,
                                                  Filter::Nearest,
                                                  SamplerAddressMode::ClampToEdge,
                                                  SamplerAddressMode::ClampToEdge,
                                                  SamplerAddressMode::ClampToEdge});
        BindGroupLayoutDesc bindings{};
        bindings.entries = {{0, BindingType::SampledTexture, ShaderStage::Fragment},
                            {1, BindingType::Sampler, ShaderStage::Fragment}};
        bindings_layout_ = device_.create_bind_group_layout(bindings);
        PipelineLayoutDesc layout{};
        layout.bind_group_layouts = {bindings_layout_};
        layout.push_constants     = {{ShaderStage::Fragment, 0, 16}};
        pipeline_layout_          = device_.create_pipeline_layout(layout);
        scene_pass_               = color_pass(device_, Format::R16G16B16A16_SFLOAT, true);
        tone_pass_                = color_pass(device_, Format::R8G8B8A8_UNORM);
        if (vertex_ && tone_shader_ && present_shader_ && sampler_ && bindings_layout_ && pipeline_layout_ &&
            scene_pass_ && tone_pass_)
        {
            tone_pipeline_ = make_pipeline(tone_pass_, tone_shader_);
        }
        if (tone_pipeline_ == INVALID_HANDLE)
        {
            ROVER_LOG_ERROR("PostProcessPipeline: initialization failed");
            shutdown();
            return false;
        }
        readback_enabled_ = enable_readback;
        initialized_      = true;
        return true;
    }

    PipelineHandle PostProcessPipeline::make_pipeline(RenderPassHandle pass, ShaderHandle fragment)
    {
        GraphicsPipelineDesc desc{};
        desc.vertex_shader   = vertex_;
        desc.fragment_shader = fragment;
        desc.render_pass     = pass;
        desc.pipeline_layout = pipeline_layout_;
        desc.cull_mode       = CullMode::None;
        return device_.create_graphics_pipeline(desc);
    }

    RenderBindGroupId PostProcessPipeline::make_bindings(FrameGraph& graph, RenderResourceId texture)
    {
        RenderBinding image{};
        image.entry.binding = 0;
        image.entry.type    = BindingType::SampledTexture;
        image.resource      = texture;
        RenderBinding sampler{};
        sampler.entry.binding = 1;
        sampler.entry.type    = BindingType::Sampler;
        sampler.entry.sampler = sampler_;
        return graph.create_bind_group(bindings_layout_, {image, sampler});
    }

    bool PostProcessPipeline::prepare()
    {
        const u32  width  = device_.get_swapchain_width();
        const u32  height = device_.get_swapchain_height();
        const auto format = device_.get_swapchain_format();
        const u32  count  = device_.get_swapchain_image_count();
        if (!initialized_ || width == 0 || height == 0 || count == 0 || format == Format::UNDEFINED ||
            static_cast<usize>(width) > std::numeric_limits<usize>::max() / height / 4)
        {
            ROVER_LOG_ERROR("PostProcessPipeline: invalid device state or target extent");
            return false;
        }
        std::vector<TextureHandle> screens;
        for (u32 i = 0; i < count; ++i)
        {
            const auto image = device_.get_swapchain_texture_at(i);
            if (!image)
            {
                return false;
            }
            screens.push_back(image);
        }
        if (width == width_ && height == height_ && format == screen_format_ && screens == screens_)
        {
            return true;
        }
        const auto next_pass     = color_pass(device_, format);
        const auto next_pipeline = next_pass ? make_pipeline(next_pass, present_shader_) : INVALID_HANDLE;
        if (!next_pipeline)
        {
            if (next_pass)
            {
                device_.destroy_render_pass(next_pass);
            }
            ROVER_LOG_ERROR("PostProcessPipeline: configuration rebuild failed; previous configuration retained");
            return false;
        }
        device_.wait_idle();
        pool_.clear();
        release(device_, present_pipeline_, &GraphicsDevice::destroy_pipeline);
        release(device_, present_pass_, &GraphicsDevice::destroy_render_pass);
        present_pass_     = next_pass;
        present_pipeline_ = next_pipeline;
        width_            = width;
        height_           = height;
        screen_format_    = format;
        screens_          = std::move(screens);
        copied_screen_    = INVALID_HANDLE;
        readback_buffer_  = INVALID_HANDLE;
        ++generation_;
        ROVER_LOG_INFO("PostProcessPipeline: {}x{}, {} images, generation {}", width_, height_, count, generation_);
        return true;
    }

    void PostProcessPipeline::draw_fullscreen(PassExecuteContext& ctx,
                                              PipelineHandle      pipeline,
                                              BindGroupHandle     bindings,
                                              u32                 width,
                                              u32                 height,
                                              PostProcessSettings settings)
    {
        struct Constants
        {
            f32 exposure;
            u32 tone_mapping;
            f32 padding[2];
        };

        static_assert(sizeof(Constants) == 16);
        const Constants constants{settings.exposure, settings.tone_mapping ? 1u : 0u, {0.0f, 0.0f}};
        ctx.device.cmd_bind_pipeline(ctx.cmd, pipeline);
        ctx.device.cmd_set_viewport(ctx.cmd, {0.0f, 0.0f, static_cast<f32>(width), static_cast<f32>(height)});
        ctx.device.cmd_set_scissor(ctx.cmd, {0, 0, width, height});
        ctx.device.cmd_bind_group(ctx.cmd, 0, bindings);
        ctx.device.cmd_push_constants(ctx.cmd, pipeline_layout_, ShaderStage::Fragment, &constants, sizeof(constants));
        ctx.device.cmd_draw(ctx.cmd, 3, 1, 0, 0);
    }

    bool PostProcessPipeline::add_passes(FrameGraph&         graph,
                                         TextureHandle       screen,
                                         PassSetupFn         scene_setup,
                                         PassExecuteFn       scene_execute,
                                         PostProcessSettings settings)
    {
        if (std::find(screens_.begin(), screens_.end(), screen) == screens_.end() ||
            !std::isfinite(settings.exposure) || settings.exposure < 0.0f)
        {
            ROVER_LOG_ERROR("PostProcessPipeline: invalid target or exposure");
            return false;
        }
        TextureDesc texture{};
        texture.width               = width_;
        texture.height              = height_;
        texture.format              = Format::R16G16B16A16_SFLOAT;
        texture.usage               = TextureUsage::ColorAttachment | TextureUsage::Sampled;
        const auto hdr              = graph.create_texture("scene.hdr", texture);
        texture.format              = Format::D32_SFLOAT;
        texture.usage               = TextureUsage::DepthStencilAttachment;
        const auto depth            = graph.create_texture("scene.depth", texture);
        texture.format              = Format::R8G8B8A8_UNORM;
        texture.usage               = TextureUsage::ColorAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
        const auto ldr              = graph.create_texture("scene.ldr", texture);
        const auto output           = graph.import_texture("screen", screen, screen_format_, width_, height_);
        const auto tone_bindings    = make_bindings(graph, hdr);
        const auto present_bindings = make_bindings(graph, ldr);
        const auto width            = width_;
        const auto height           = height_;
        // Consumers intentionally register first; compilation derives the order.
        const auto present = graph.add_pass(
            "present",
            [=](PassBuilder& b) { b.read(ldr); },
            [this, present_bindings, width, height, settings](PassExecuteContext& ctx) {
                draw_fullscreen(
                    ctx, present_pipeline_, ctx.graph.bind_group(present_bindings), width, height, settings);
            });
        graph.set_render_target(present, present_pass_, output, {});
        const auto tone = graph.add_pass(
            "tone_map",
            [=](PassBuilder& b) { b.read(hdr); },
            [this, tone_bindings, width, height, settings](PassExecuteContext& ctx) {
                draw_fullscreen(ctx, tone_pipeline_, ctx.graph.bind_group(tone_bindings), width, height, settings);
            });
        graph.set_render_target(tone, tone_pass_, ldr, {});
        const auto scene = graph.add_pass(
            "forward",
            [=](PassBuilder& b) {
                if (scene_setup)
                {
                    scene_setup(b);
                }
            },
            std::move(scene_execute));
        ClearValue clear{};
        clear.color[0] = 0.05f;
        clear.color[1] = 0.07f;
        clear.color[2] = 0.10f;
        graph.set_render_target(scene, scene_pass_, hdr, clear, depth);
        copied_screen_ = INVALID_HANDLE;
        if (readback_enabled_)
        {
            const auto buffer = graph.create_buffer(
                "scene.readback",
                {static_cast<usize>(width_) * height_ * 4, BufferUsage::Transfer, MemoryUsage::GpuToCpu, nullptr});
            graph.add_pass(
                "readback",
                [=](PassBuilder& b) {
                    b.read(ldr, ResourceAccess::TransferRead);
                    b.write(buffer, ResourceAccess::TransferWrite);
                },
                [this, screen, ldr, buffer](PassExecuteContext& ctx) {
                    if (ctx.device.cmd_copy_texture_to_buffer(
                            ctx.cmd, ctx.graph.resource(ldr).texture, ctx.graph.resource(buffer).buffer))
                    {
                        copied_screen_   = screen;
                        readback_buffer_ = ctx.graph.resource(buffer).buffer;
                    }
                });
            graph.set_final_access(buffer, ResourceAccess::HostRead);
        }
        return true;
    }

    bool PostProcessPipeline::read_pixels(TextureHandle screen, std::vector<u8>& pixels)
    {
        if (!readback_buffer_ || !readback_enabled_ || copied_screen_ != screen)
        {
            ROVER_LOG_ERROR("PostProcessPipeline: no completed copy for this image");
            return false;
        }
        device_.wait_idle();
        const void* data = device_.map_buffer(readback_buffer_);
        if (!data)
        {
            ROVER_LOG_ERROR("PostProcessPipeline: could not map readback");
            return false;
        }
        pixels.resize(static_cast<usize>(width_) * height_ * 4);
        std::memcpy(pixels.data(), data, pixels.size());
        device_.unmap_buffer(readback_buffer_);
        return true;
    }

    void PostProcessPipeline::shutdown()
    {
        if (initialized_ || vertex_ || tone_shader_ || present_shader_ || scene_pass_ || tone_pass_)
        {
            device_.wait_idle();
        }
        pool_.clear();
        screens_.clear();
        readback_buffer_ = INVALID_HANDLE;
        release(device_, present_pipeline_, &GraphicsDevice::destroy_pipeline);
        release(device_, tone_pipeline_, &GraphicsDevice::destroy_pipeline);
        release(device_, pipeline_layout_, &GraphicsDevice::destroy_pipeline_layout);
        release(device_, bindings_layout_, &GraphicsDevice::destroy_bind_group_layout);
        release(device_, sampler_, &GraphicsDevice::destroy_sampler);
        release(device_, vertex_, &GraphicsDevice::destroy_shader);
        release(device_, tone_shader_, &GraphicsDevice::destroy_shader);
        release(device_, present_shader_, &GraphicsDevice::destroy_shader);
        release(device_, present_pass_, &GraphicsDevice::destroy_render_pass);
        release(device_, tone_pass_, &GraphicsDevice::destroy_render_pass);
        release(device_, scene_pass_, &GraphicsDevice::destroy_render_pass);
        width_ = height_ = 0;
        initialized_ = readback_enabled_ = false;
        copied_screen_                   = INVALID_HANDLE;
        screen_format_                   = Format::UNDEFINED;
    }
} // namespace rover
