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

    BindGroupHandle PostProcessPipeline::make_bindings(TextureHandle texture)
    {
        BindGroupDesc desc{};
        desc.layout = bindings_layout_;
        BindGroupEntry image{};
        image.binding = 0;
        image.type    = BindingType::SampledTexture;
        image.texture = texture;
        BindGroupEntry sampler{};
        sampler.binding = 1;
        sampler.type    = BindingType::Sampler;
        sampler.sampler = sampler_;
        desc.entries    = {image, sampler};
        return device_.create_bind_group(desc);
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
        bool matches = width == width_ && height == height_ && format == screen_format_ && targets_.size() == count;
        for (u32 i = 0; i < count && matches; ++i)
        {
            matches = targets_[i].screen == device_.get_swapchain_texture_at(i);
        }
        if (matches)
        {
            return true;
        }

        device_.wait_idle();
        std::vector<Target> next;
        const auto          next_pass     = color_pass(device_, format);
        const auto          next_pipeline = next_pass ? make_pipeline(next_pass, present_shader_) : INVALID_HANDLE;
        auto                fail          = [&]() {
            destroy_targets(next);
            if (next_pipeline)
            {
                device_.destroy_pipeline(next_pipeline);
            }
            if (next_pass)
            {
                device_.destroy_render_pass(next_pass);
            }
            ROVER_LOG_ERROR("PostProcessPipeline: target rebuild failed; previous targets retained");
            return false;
        };
        if (!next_pipeline)
        {
            return fail();
        }
        next.reserve(count);
        for (u32 i = 0; i < count; ++i)
        {
            auto& target  = next.emplace_back();
            target.screen = device_.get_swapchain_texture_at(i);
            if (!target.screen)
            {
                return fail();
            }
            TextureDesc texture{};
            texture.width  = width;
            texture.height = height;
            texture.format = Format::R16G16B16A16_SFLOAT;
            texture.usage  = TextureUsage::ColorAttachment | TextureUsage::Sampled;
            target.hdr     = device_.create_texture(texture);
            texture.format = Format::D32_SFLOAT;
            texture.usage  = TextureUsage::DepthStencilAttachment;
            target.depth   = device_.create_texture(texture);
            texture.format = Format::R8G8B8A8_UNORM;
            texture.usage  = TextureUsage::ColorAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
            target.ldr     = device_.create_texture(texture);
            if (!target.hdr || !target.depth || !target.ldr)
            {
                return fail();
            }
            FramebufferDesc framebuffer{};
            framebuffer.width             = width;
            framebuffer.height            = height;
            framebuffer.render_pass       = scene_pass_;
            framebuffer.color_attachments = {target.hdr};
            framebuffer.depth_stencil     = target.depth;
            target.scene_fb               = device_.create_framebuffer(framebuffer);
            framebuffer.render_pass       = tone_pass_;
            framebuffer.color_attachments = {target.ldr};
            framebuffer.depth_stencil     = INVALID_HANDLE;
            target.tone_fb                = device_.create_framebuffer(framebuffer);
            framebuffer.render_pass       = next_pass;
            framebuffer.color_attachments = {target.screen};
            target.present_fb             = device_.create_framebuffer(framebuffer);
            target.tone_bindings          = make_bindings(target.hdr);
            target.present_bindings       = make_bindings(target.ldr);
            if (readback_enabled_)
            {
                target.readback = device_.create_buffer({static_cast<usize>(width) * height * 4,
                                                         BufferUsage::Transfer,
                                                         MemoryUsage::GpuToCpu,
                                                         "SceneReadback"});
                if (!target.readback)
                {
                    return fail();
                }
            }
            if (!target.scene_fb || !target.tone_fb || !target.present_fb || !target.tone_bindings ||
                !target.present_bindings)
            {
                return fail();
            }
        }
        destroy_targets(targets_);
        release(device_, present_pipeline_, &GraphicsDevice::destroy_pipeline);
        release(device_, present_pass_, &GraphicsDevice::destroy_render_pass);
        targets_          = std::move(next);
        present_pass_     = next_pass;
        present_pipeline_ = next_pipeline;
        width_            = width;
        height_           = height;
        screen_format_    = format;
        copied_screen_    = INVALID_HANDLE;
        ++generation_;
        ROVER_LOG_INFO("PostProcessPipeline: {}x{}, {} images, generation {}", width_, height_, count, generation_);
        return true;
    }

    PostProcessPipeline::Target* PostProcessPipeline::find_target(TextureHandle screen)
    {
        auto it =
            std::find_if(targets_.begin(), targets_.end(), [&](const auto& target) { return target.screen == screen; });
        return it == targets_.end() ? nullptr : &*it;
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
        const auto* target = find_target(screen);
        if (!target || !std::isfinite(settings.exposure) || settings.exposure < 0.0f)
        {
            ROVER_LOG_ERROR("PostProcessPipeline: invalid target or exposure");
            return false;
        }
        const auto hdr = graph.import_texture(
            "scene.hdr", target->hdr, Format::R16G16B16A16_SFLOAT, width_, height_, target->scene_fb);
        const auto depth = graph.import_texture("scene.depth", target->depth, Format::D32_SFLOAT, width_, height_);
        const auto ldr =
            graph.import_texture("scene.ldr", target->ldr, Format::R8G8B8A8_UNORM, width_, height_, target->tone_fb);
        const auto output = graph.import_texture("screen", screen, screen_format_, width_, height_, target->present_fb);
        const auto width  = width_;
        const auto height = height_;
        // Consumers intentionally register first; compilation derives the order.
        const auto present = graph.add_pass(
            "present",
            [=](PassBuilder& b) { b.read(ldr); },
            [this, bindings = target->present_bindings, width, height, settings](PassExecuteContext& ctx) {
                draw_fullscreen(ctx, present_pipeline_, bindings, width, height, settings);
            });
        graph.set_color_attachment(present, output, {});
        const auto tone = graph.add_pass(
            "tone_map",
            [=](PassBuilder& b) { b.read(hdr); },
            [this, bindings = target->tone_bindings, width, height, settings](PassExecuteContext& ctx) {
                draw_fullscreen(ctx, tone_pipeline_, bindings, width, height, settings);
            });
        graph.set_color_attachment(tone, ldr, {});
        const auto scene = graph.add_pass(
            "forward",
            [=](PassBuilder& b) {
                b.write(depth, ResourceAccess::DepthStencilAttachment);
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
        graph.set_color_attachment(scene, hdr, clear, true);
        copied_screen_ = INVALID_HANDLE;
        if (readback_enabled_)
        {
            const auto buffer = graph.import_buffer("scene.readback", target->readback);
            graph.add_pass(
                "readback",
                [=](PassBuilder& b) {
                    b.read(ldr, ResourceAccess::TransferRead);
                    b.write(buffer, ResourceAccess::TransferWrite);
                },
                [this, screen, texture = target->ldr, buffer = target->readback](PassExecuteContext& ctx) {
                    if (ctx.device.cmd_copy_texture_to_buffer(ctx.cmd, texture, buffer))
                    {
                        copied_screen_ = screen;
                    }
                });
            graph.set_final_access(buffer, ResourceAccess::HostRead);
        }
        return true;
    }

    bool PostProcessPipeline::read_pixels(TextureHandle screen, std::vector<u8>& pixels)
    {
        const auto* target = find_target(screen);
        if (!target || !readback_enabled_ || copied_screen_ != screen)
        {
            ROVER_LOG_ERROR("PostProcessPipeline: no completed copy for this image");
            return false;
        }
        device_.wait_idle();
        const void* data = device_.map_buffer(target->readback);
        if (!data)
        {
            ROVER_LOG_ERROR("PostProcessPipeline: could not map readback");
            return false;
        }
        pixels.resize(static_cast<usize>(width_) * height_ * 4);
        std::memcpy(pixels.data(), data, pixels.size());
        device_.unmap_buffer(target->readback);
        return true;
    }

    void PostProcessPipeline::destroy_targets(std::vector<Target>& targets)
    {
        for (auto& target : targets)
        {
            release(device_, target.tone_bindings, &GraphicsDevice::destroy_bind_group);
            release(device_, target.present_bindings, &GraphicsDevice::destroy_bind_group);
            release(device_, target.scene_fb, &GraphicsDevice::destroy_framebuffer);
            release(device_, target.tone_fb, &GraphicsDevice::destroy_framebuffer);
            release(device_, target.present_fb, &GraphicsDevice::destroy_framebuffer);
            release(device_, target.hdr, &GraphicsDevice::destroy_texture);
            release(device_, target.depth, &GraphicsDevice::destroy_texture);
            release(device_, target.ldr, &GraphicsDevice::destroy_texture);
            release(device_, target.readback, &GraphicsDevice::destroy_buffer);
        }
        targets.clear();
    }

    void PostProcessPipeline::shutdown()
    {
        if (initialized_ || vertex_ || tone_shader_ || present_shader_ || scene_pass_ || tone_pass_)
        {
            device_.wait_idle();
        }
        destroy_targets(targets_);
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
