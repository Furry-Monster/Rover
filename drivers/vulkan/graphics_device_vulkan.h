#pragma once

#include "core/graphics/graphics_desc.h"
#include "core/graphics/graphics_device.h"
#include "core/graphics/graphics_types.h"
#include "core/graphics/window_system.h"
#include "core/typedefs.h"
#include "drivers/vulkan/vk_common.h"
#include "drivers/vulkan/vk_device.h"
#include "drivers/vulkan/vk_instance.h"
#include "drivers/vulkan/vk_resource_pool.h"
#include "drivers/vulkan/vk_swapchain.h"

#include <vk_mem_alloc.h>

#include <array>
#include <vector>

namespace rover
{

    struct VkBufferResource
    {
        VkBuffer      buffer     = VK_NULL_HANDLE;
        VmaAllocation allocation = nullptr;
        VkDeviceSize  size       = 0;
        bool          mapped     = false;
        void*         mapped_ptr = nullptr;
        BufferUsage   usage      = BufferUsage::Vertex;
        MemoryUsage   memory     = MemoryUsage::GpuOnly;
    };

    struct VkTextureResource
    {
        VkImage       image        = VK_NULL_HANDLE;
        VkImageView   view         = VK_NULL_HANDLE;
        VmaAllocation allocation   = nullptr;
        VkFormat      format       = VK_FORMAT_UNDEFINED;
        VkExtent3D    extent       = {};
        u32           mip_levels   = 1;
        u32           array_layers = 1;
        bool          owns_image   = true;
    };

    struct VkShaderResource
    {
        VkShaderModule        module = VK_NULL_HANDLE;
        VkShaderStageFlagBits stage  = VK_SHADER_STAGE_VERTEX_BIT;
    };

    struct VkRenderPassResource
    {
        VkRenderPass        pass = VK_NULL_HANDLE;
        std::vector<Format> color_formats;
        bool                has_depth = false;
    };

    struct VkFramebufferResource
    {
        VkFramebuffer    framebuffer      = VK_NULL_HANDLE;
        u32              width            = 0;
        u32              height           = 0;
        u32              attachment_count = 0;
        RenderPassHandle render_pass      = INVALID_HANDLE;
    };

    struct VkPipelineResource
    {
        VkPipeline           pipeline    = VK_NULL_HANDLE;
        PipelineLayoutHandle layout      = INVALID_HANDLE;
        bool                 owns_layout = false;
    };

    struct VkBindGroupLayoutResource
    {
        VkDescriptorSetLayout             layout = VK_NULL_HANDLE;
        std::vector<BindGroupLayoutEntry> entries;
    };

    struct VkBindGroupResource
    {
        VkDescriptorSet       set    = VK_NULL_HANDLE;
        VkDescriptorPool      pool   = VK_NULL_HANDLE;
        BindGroupLayoutHandle layout = INVALID_HANDLE;
    };

    struct VkPipelineLayoutResource
    {
        VkPipelineLayout                   layout = VK_NULL_HANDLE;
        std::vector<BindGroupLayoutHandle> bind_group_layouts;
        std::vector<PushConstantRange>     push_constants;
    };

    struct VkCommandListResource
    {
        VkCommandBuffer buffer    = VK_NULL_HANDLE;
        bool            recording = false;
        bool            submitted = false;
        // Pipeline layout of the most recently bound pipeline; captured at
        // `cmd_bind_pipeline` so subsequent `cmd_bind_group` / `cmd_push_constants`
        // calls can use it without crossing back through the abstract API.
        VkPipelineLayout current_layout = VK_NULL_HANDLE;
    };

    // Per-frame-in-flight CPU/GPU sync. The render_finished semaphore is NOT
    // stored here -- it is owned by VkSwapchainWrapper per swapchain image,
    // because vkQueuePresentKHR's wait-semaphore lifetime is image-bound, not
    // frame-slot-bound.
    struct FrameSync
    {
        VkSemaphore image_available = VK_NULL_HANDLE;
        VkFence     in_flight       = VK_NULL_HANDLE;
    };

    class GraphicsDeviceVulkan : public GraphicsDevice
    {
    public:
        GraphicsDeviceVulkan();
        ~GraphicsDeviceVulkan() override;

        bool init(WindowSystem& window) override;
        void shutdown() override;

        BufferHandle create_buffer(const BufferDesc& desc) override;
        void         destroy_buffer(BufferHandle handle) override;
        void*        map_buffer(BufferHandle handle) override;
        void         unmap_buffer(BufferHandle handle) override;
        void         update_buffer(BufferHandle handle, const void* data, usize size, usize offset = 0) override;

        TextureHandle create_texture(const TextureDesc& desc) override;
        void          destroy_texture(TextureHandle handle) override;
        void          update_texture(TextureHandle handle, const void* data, usize size) override;

        SamplerHandle create_sampler(const SamplerDesc& desc) override;
        void          destroy_sampler(SamplerHandle handle) override;

        ShaderHandle create_shader(const ShaderDesc& desc) override;
        void         destroy_shader(ShaderHandle handle) override;

        RenderPassHandle create_render_pass(const RenderPassDesc& desc) override;
        void             destroy_render_pass(RenderPassHandle handle) override;

        FramebufferHandle create_framebuffer(const FramebufferDesc& desc) override;
        void              destroy_framebuffer(FramebufferHandle handle) override;

        BindGroupLayoutHandle create_bind_group_layout(const BindGroupLayoutDesc& desc) override;
        void                  destroy_bind_group_layout(BindGroupLayoutHandle handle) override;

        BindGroupHandle create_bind_group(const BindGroupDesc& desc) override;
        void            destroy_bind_group(BindGroupHandle handle) override;

        PipelineLayoutHandle create_pipeline_layout(const PipelineLayoutDesc& desc) override;
        void                 destroy_pipeline_layout(PipelineLayoutHandle handle) override;

        PipelineHandle create_graphics_pipeline(const GraphicsPipelineDesc& desc) override;
        void           destroy_pipeline(PipelineHandle handle) override;

        CommandListHandle begin_command_list() override;
        void              end_command_list(CommandListHandle cmd) override;

        void cmd_begin_render_pass(CommandListHandle cmd,
                                   FramebufferHandle fb,
                                   const ClearValue* clear_values,
                                   u32               clear_count) override;
        void cmd_end_render_pass(CommandListHandle cmd) override;

        void cmd_bind_pipeline(CommandListHandle cmd, PipelineHandle pipeline) override;
        void cmd_set_viewport(CommandListHandle cmd, const Viewport& viewport) override;
        void cmd_set_scissor(CommandListHandle cmd, const Scissor& scissor) override;
        void cmd_bind_group(CommandListHandle cmd, u32 set_index, BindGroupHandle group) override;
        void cmd_push_constants(CommandListHandle    cmd,
                                PipelineLayoutHandle layout,
                                ShaderStage          stage,
                                const void*          data,
                                u32                  size,
                                u32                  offset = 0) override;

        void cmd_bind_vertex_buffer(CommandListHandle cmd,
                                    BufferHandle      buffer,
                                    u32               binding = 0,
                                    usize             offset  = 0) override;
        void cmd_bind_index_buffer(CommandListHandle cmd,
                                   BufferHandle      buffer,
                                   IndexType         type,
                                   usize             offset = 0) override;

        void cmd_draw(CommandListHandle cmd,
                      u32               vertex_count,
                      u32               instance_count = 1,
                      u32               first_vertex   = 0,
                      u32               first_instance = 0) override;
        void cmd_draw_indexed(CommandListHandle cmd,
                              u32               index_count,
                              u32               instance_count = 1,
                              u32               first_index    = 0,
                              i32               vertex_offset  = 0,
                              u32               first_instance = 0) override;

        bool begin_frame() override;
        void end_frame() override;
        void present() override;
        void wait_idle() override;

        TextureHandle get_swapchain_texture() override;
        u32           get_swapchain_image_count() override;
        TextureHandle get_swapchain_texture_at(u32 image_index) override;
        Format        get_swapchain_format() override;
        u32           get_swapchain_width() override;
        u32           get_swapchain_height() override;

        [[nodiscard]] const char* get_device_name() const override;
        [[nodiscard]] const char* get_api_name() const override;

    private:
        void recreate_swapchain();
        void register_swapchain_textures();
        void unregister_swapchain_textures();

        WindowSystem*      window_  = nullptr;
        VkSurfaceKHR       surface_ = VK_NULL_HANDLE;
        VkInstanceWrapper  instance_;
        VkDeviceWrapper    device_;
        VmaAllocator       vma_allocator_ = nullptr;
        VkSwapchainWrapper swapchain_;

        VkCommandPool command_pool_ = VK_NULL_HANDLE;

        std::array<FrameSync, kMaxFramesInFlight> frame_sync_{};
        // Command buffers retired with each frame slot. Freed at the start of
        // begin_frame() AFTER the in-flight fence has signaled, guaranteeing the
        // GPU is no longer reading them.
        std::array<std::vector<CommandListHandle>, kMaxFramesInFlight> retired_command_lists_{};
        u32                                                            current_frame_ = 0;
        u32                                                            current_image_ = 0;

        VkResourcePool<VkBufferResource>          buffers_;
        VkResourcePool<VkTextureResource>         textures_;
        VkResourcePool<VkSampler>                 samplers_;
        VkResourcePool<VkShaderResource>          shaders_;
        VkResourcePool<VkRenderPassResource>      render_passes_;
        VkResourcePool<VkFramebufferResource>     framebuffers_;
        VkResourcePool<VkPipelineResource>        pipelines_;
        VkResourcePool<VkPipelineLayoutResource>  pipeline_layouts_;
        VkResourcePool<VkBindGroupLayoutResource> bind_group_layouts_;
        VkResourcePool<VkBindGroupResource>       bind_groups_;
        VkResourcePool<VkCommandListResource>     command_lists_;

        // Long-lived descriptor pool (Phase 2 simple impl: single growable pool).
        VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;

        std::vector<TextureHandle>     swapchain_texture_handles_;
        std::vector<CommandListHandle> frame_command_lists_;

        bool in_frame_        = false;
        bool swapchain_dirty_ = false;
        char api_name_[32]    = "Vulkan 1.3";
    };

} // namespace rover
