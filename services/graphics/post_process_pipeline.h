#pragma once

#include "core/graphics/graphics_device.h"
#include "services/graphics/frame_graph.h"

#include <vector>

namespace rover
{
    struct PostProcessSettings
    {
        f32  exposure     = 1.0f;
        bool tone_mapping = true;
    };

    // Owns per-swapchain-image HDR/depth/LDR targets. ECS draw commands are
    // supplied by the caller; this service depends only on graphics abstractions.
    class PostProcessPipeline
    {
    public:
        explicit PostProcessPipeline(GraphicsDevice& device) : device_(device) {}

        ~PostProcessPipeline();
        PostProcessPipeline(const PostProcessPipeline&)            = delete;
        PostProcessPipeline& operator=(const PostProcessPipeline&) = delete;

        bool initialize(const ShaderDesc& vertex,
                        const ShaderDesc& tone_map,
                        const ShaderDesc& present,
                        bool              enable_readback = false);
        // Call after begin_frame(). Rebuilds transactionally after GPU idle when
        // image handles, extent or format change. Zero extents are rejected.
        bool prepare();
        bool add_passes(FrameGraph&         graph,
                        TextureHandle       screen,
                        PassSetupFn         scene_setup,
                        PassExecuteFn       scene_execute,
                        PostProcessSettings settings = {});
        // Call after end_frame()/present(); waits for GPU completion before map.
        bool read_pixels(TextureHandle screen, std::vector<u8>& pixels);
        void shutdown();

        [[nodiscard]] RenderPassHandle scene_render_pass() const { return scene_pass_; }

        [[nodiscard]] u32 width() const { return width_; }

        [[nodiscard]] u32 height() const { return height_; }

        [[nodiscard]] u64 generation() const { return generation_; }

        [[nodiscard]] bool readback_enabled() const { return readback_enabled_; }

    private:
        struct Target
        {
            TextureHandle     screen           = INVALID_HANDLE;
            TextureHandle     hdr              = INVALID_HANDLE;
            TextureHandle     depth            = INVALID_HANDLE;
            TextureHandle     ldr              = INVALID_HANDLE;
            FramebufferHandle scene_fb         = INVALID_HANDLE;
            FramebufferHandle tone_fb          = INVALID_HANDLE;
            FramebufferHandle present_fb       = INVALID_HANDLE;
            BindGroupHandle   tone_bindings    = INVALID_HANDLE;
            BindGroupHandle   present_bindings = INVALID_HANDLE;
            BufferHandle      readback         = INVALID_HANDLE;
        };

        Target*         find_target(TextureHandle screen);
        BindGroupHandle make_bindings(TextureHandle texture);
        PipelineHandle  make_pipeline(RenderPassHandle pass, ShaderHandle fragment);
        void            destroy_targets(std::vector<Target>& targets);
        void            draw_fullscreen(PassExecuteContext& ctx,
                                        PipelineHandle      pipeline,
                                        BindGroupHandle     bindings,
                                        u32                 width,
                                        u32                 height,
                                        PostProcessSettings settings);

        GraphicsDevice&       device_;
        std::vector<Target>   targets_;
        ShaderHandle          vertex_           = INVALID_HANDLE;
        ShaderHandle          tone_shader_      = INVALID_HANDLE;
        ShaderHandle          present_shader_   = INVALID_HANDLE;
        SamplerHandle         sampler_          = INVALID_HANDLE;
        BindGroupLayoutHandle bindings_layout_  = INVALID_HANDLE;
        PipelineLayoutHandle  pipeline_layout_  = INVALID_HANDLE;
        RenderPassHandle      scene_pass_       = INVALID_HANDLE;
        RenderPassHandle      tone_pass_        = INVALID_HANDLE;
        RenderPassHandle      present_pass_     = INVALID_HANDLE;
        PipelineHandle        tone_pipeline_    = INVALID_HANDLE;
        PipelineHandle        present_pipeline_ = INVALID_HANDLE;
        Format                screen_format_    = Format::UNDEFINED;
        u32                   width_            = 0;
        u32                   height_           = 0;
        u64                   generation_       = 0;
        bool                  initialized_      = false;
        bool                  readback_enabled_ = false;
        TextureHandle         copied_screen_    = INVALID_HANDLE;
    };
} // namespace rover
