#pragma once

#include "core/graphics/graphics_desc.h"
#include "core/graphics/window_system.h"

namespace rover {

class GraphicsDevice {
public:
    virtual ~GraphicsDevice() = default;

    // Lifecycle
    virtual bool init(WindowSystem& window) = 0;
    virtual void shutdown()                 = 0;

    // ---- Resource creation / destruction ----

    virtual BufferHandle create_buffer(const BufferDesc& desc)                              = 0;
    virtual void         destroy_buffer(BufferHandle handle)                                = 0;
    virtual void*        map_buffer(BufferHandle handle)                                    = 0;
    virtual void         unmap_buffer(BufferHandle handle)                                  = 0;
    virtual void         update_buffer(BufferHandle handle, const void* data,
                                       usize size, usize offset = 0)                       = 0;

    virtual TextureHandle create_texture(const TextureDesc& desc)                           = 0;
    virtual void          destroy_texture(TextureHandle handle)                             = 0;
    virtual void          update_texture(TextureHandle handle, const void* data, usize size) = 0;

    virtual SamplerHandle create_sampler(const SamplerDesc& desc)                           = 0;
    virtual void          destroy_sampler(SamplerHandle handle)                             = 0;

    virtual ShaderHandle create_shader(const ShaderDesc& desc)                              = 0;
    virtual void         destroy_shader(ShaderHandle handle)                                = 0;

    virtual RenderPassHandle create_render_pass(const RenderPassDesc& desc)                 = 0;
    virtual void             destroy_render_pass(RenderPassHandle handle)                   = 0;

    virtual FramebufferHandle create_framebuffer(const FramebufferDesc& desc)               = 0;
    virtual void              destroy_framebuffer(FramebufferHandle handle)                 = 0;

    virtual PipelineHandle create_graphics_pipeline(const GraphicsPipelineDesc& desc)       = 0;
    virtual void           destroy_pipeline(PipelineHandle handle)                          = 0;

    // ---- Command recording ----

    virtual CommandListHandle begin_command_list()                                          = 0;
    virtual void              end_command_list(CommandListHandle cmd)                       = 0;

    virtual void cmd_begin_render_pass(CommandListHandle cmd, FramebufferHandle fb,
                                       const ClearValue* clear_values, u32 clear_count)    = 0;
    virtual void cmd_end_render_pass(CommandListHandle cmd)                                 = 0;

    virtual void cmd_bind_pipeline(CommandListHandle cmd, PipelineHandle pipeline)          = 0;
    virtual void cmd_set_viewport(CommandListHandle cmd, const Viewport& viewport)          = 0;
    virtual void cmd_set_scissor(CommandListHandle cmd, const Scissor& scissor)             = 0;

    virtual void cmd_bind_vertex_buffer(CommandListHandle cmd, BufferHandle buffer,
                                        u32 binding = 0, usize offset = 0)                 = 0;
    virtual void cmd_bind_index_buffer(CommandListHandle cmd, BufferHandle buffer,
                                       IndexType type, usize offset = 0)                   = 0;

    virtual void cmd_draw(CommandListHandle cmd, u32 vertex_count,
                          u32 instance_count = 1, u32 first_vertex = 0,
                          u32 first_instance = 0)                                           = 0;
    virtual void cmd_draw_indexed(CommandListHandle cmd, u32 index_count,
                                  u32 instance_count = 1, u32 first_index = 0,
                                  i32 vertex_offset = 0, u32 first_instance = 0)           = 0;

    // ---- Frame management ----

    virtual bool begin_frame() = 0;
    virtual void end_frame()   = 0;
    virtual void present()     = 0;
    virtual void wait_idle()   = 0;

    // ---- Swapchain ----

    // Returns the texture handle for the swapchain image currently bound for
    // rendering this frame (valid between begin_frame() and present()).
    virtual TextureHandle get_swapchain_texture() = 0;

    // Enumeration helpers so callers can pre-create framebuffers per image.
    virtual u32           get_swapchain_image_count()                 = 0;
    virtual TextureHandle get_swapchain_texture_at(u32 image_index)   = 0;

    virtual Format        get_swapchain_format()  = 0;
    virtual u32           get_swapchain_width()   = 0;
    virtual u32           get_swapchain_height()  = 0;

    // ---- Info ----

    virtual const char* get_device_name() const = 0;
    virtual const char* get_api_name() const    = 0;
};

} // namespace rover
