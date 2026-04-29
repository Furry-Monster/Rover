#include "core/log/log.h"
#include "drivers/vulkan/graphics_device_vulkan.h"
#include "drivers/vulkan/vk_format.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace rover
{

    GraphicsDeviceVulkan::GraphicsDeviceVulkan() = default;

    GraphicsDeviceVulkan::~GraphicsDeviceVulkan()
    {
        if (device_.logical() != VK_NULL_HANDLE)
        {
            shutdown();
        }
    }

    bool GraphicsDeviceVulkan::init(WindowSystem& window)
    {
        window_ = &window;

        std::vector<const char*> required_extensions;
        window.get_vulkan_required_extensions(required_extensions);

#ifdef ROVER_DEBUG
        constexpr bool enable_validation = true;
#else
        constexpr bool enable_validation = false;
#endif

        if (!instance_.init(required_extensions, enable_validation))
        {
            ROVER_LOG_ERROR("Failed to create Vulkan instance");
            return false;
        }

        void* surface_raw = nullptr;
        if (!window.create_vulkan_surface(instance_.handle(), &surface_raw))
        {
            ROVER_LOG_ERROR("Failed to create Vulkan surface");
            return false;
        }
        surface_ = reinterpret_cast<VkSurfaceKHR>(surface_raw);

        if (!device_.init(instance_.handle(), surface_))
        {
            ROVER_LOG_ERROR("Failed to create Vulkan device");
            return false;
        }

        VmaVulkanFunctions vma_funcs{};
        vma_funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vma_funcs.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocator_info{};
        allocator_info.physicalDevice   = device_.physical();
        allocator_info.device           = device_.logical();
        allocator_info.instance         = instance_.handle();
        allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;
        allocator_info.pVulkanFunctions = &vma_funcs;
        VK_CHECK_RETURN(vmaCreateAllocator(&allocator_info, &vma_allocator_), false);

        if (!swapchain_.init(instance_.handle(),
                             device_.physical(),
                             device_.logical(),
                             surface_,
                             device_.queue_families(),
                             window.get_width(),
                             window.get_height()))
        {
            ROVER_LOG_ERROR("Failed to create Vulkan swapchain");
            return false;
        }
        register_swapchain_textures();

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = device_.queue_families().graphics;
        VK_CHECK_RETURN(vkCreateCommandPool(device_.logical(), &pool_info, nullptr, &command_pool_), false);

        // Single global descriptor pool sized for typical Phase 2 / Phase 3 demos.
        // Pool fragmentation will be revisited when material systems land.
        {
            VkDescriptorPoolSize pool_sizes[] = {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 512},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 128},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 256},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 64},
                {VK_DESCRIPTOR_TYPE_SAMPLER, 64},
            };
            VkDescriptorPoolCreateInfo dp{};
            dp.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            dp.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            dp.maxSets       = 1024;
            dp.poolSizeCount = static_cast<u32>(sizeof(pool_sizes) / sizeof(pool_sizes[0]));
            dp.pPoolSizes    = pool_sizes;
            VK_CHECK_RETURN(vkCreateDescriptorPool(device_.logical(), &dp, nullptr, &descriptor_pool_), false);
        }

        VkSemaphoreCreateInfo sem_info{};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (u32 i = 0; i < kMaxFramesInFlight; ++i)
        {
            VK_CHECK_RETURN(vkCreateSemaphore(device_.logical(), &sem_info, nullptr, &frame_sync_[i].image_available),
                            false);
            VK_CHECK_RETURN(vkCreateFence(device_.logical(), &fence_info, nullptr, &frame_sync_[i].in_flight), false);
        }

        ROVER_LOG_INFO("GraphicsDeviceVulkan initialized");
        return true;
    }

    void GraphicsDeviceVulkan::shutdown()
    {
        if (device_.logical() == VK_NULL_HANDLE)
        {
            return;
        }

        vkDeviceWaitIdle(device_.logical());

        pipelines_.for_each([&](u64, VkPipelineResource& res) {
            if (res.pipeline != VK_NULL_HANDLE)
            {
                vkDestroyPipeline(device_.logical(), res.pipeline, nullptr);
            }
            // Owned pipeline layouts (created implicitly with empty layout) live
            // in pipeline_layouts_ now; nothing to free here.
        });
        pipelines_.clear();

        // Bind groups must be freed before their pool is destroyed; bind group
        // layouts and pipeline layouts are independent.
        bind_groups_.for_each([&](u64, VkBindGroupResource& res) {
            if (res.set != VK_NULL_HANDLE && res.pool != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(device_.logical(), res.pool, 1, &res.set);
            }
        });
        bind_groups_.clear();

        bind_group_layouts_.for_each([&](u64, VkBindGroupLayoutResource& res) {
            if (res.layout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(device_.logical(), res.layout, nullptr);
            }
        });
        bind_group_layouts_.clear();

        pipeline_layouts_.for_each([&](u64, VkPipelineLayoutResource& res) {
            if (res.layout != VK_NULL_HANDLE)
            {
                vkDestroyPipelineLayout(device_.logical(), res.layout, nullptr);
            }
        });
        pipeline_layouts_.clear();

        if (descriptor_pool_ != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device_.logical(), descriptor_pool_, nullptr);
            descriptor_pool_ = VK_NULL_HANDLE;
        }

        framebuffers_.for_each([&](u64, VkFramebufferResource& res) {
            if (res.framebuffer != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(device_.logical(), res.framebuffer, nullptr);
            }
        });
        framebuffers_.clear();

        render_passes_.for_each([&](u64, VkRenderPassResource& res) {
            if (res.pass != VK_NULL_HANDLE)
            {
                vkDestroyRenderPass(device_.logical(), res.pass, nullptr);
            }
        });
        render_passes_.clear();

        shaders_.for_each([&](u64, VkShaderResource& res) {
            if (res.module != VK_NULL_HANDLE)
            {
                vkDestroyShaderModule(device_.logical(), res.module, nullptr);
            }
        });
        shaders_.clear();

        samplers_.for_each([&](u64, VkSampler& sampler) {
            if (sampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(device_.logical(), sampler, nullptr);
            }
        });
        samplers_.clear();

        textures_.for_each([&](u64, VkTextureResource& res) {
            // Swapchain-owned textures (owns_image == false) have their image
            // and view destroyed by VkSwapchainWrapper::destroy_internal below;
            // double-freeing here would corrupt the Vulkan allocator state.
            if (!res.owns_image)
            {
                return;
            }
            if (res.view != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device_.logical(), res.view, nullptr);
            }
            if (res.image != VK_NULL_HANDLE && res.allocation != nullptr)
            {
                vmaDestroyImage(vma_allocator_, res.image, res.allocation);
            }
            else if (res.image != VK_NULL_HANDLE)
            {
                vkDestroyImage(device_.logical(), res.image, nullptr);
            }
        });
        textures_.clear();
        swapchain_texture_handles_.clear();

        buffers_.for_each([&](u64, VkBufferResource& res) {
            if (res.mapped && res.allocation != nullptr)
            {
                vmaUnmapMemory(vma_allocator_, res.allocation);
            }
            if (res.buffer != VK_NULL_HANDLE && res.allocation != nullptr)
            {
                vmaDestroyBuffer(vma_allocator_, res.buffer, res.allocation);
            }
        });
        buffers_.clear();

        command_lists_.for_each([&](u64, VkCommandListResource& res) {
            if (res.buffer != VK_NULL_HANDLE && command_pool_ != VK_NULL_HANDLE)
            {
                vkFreeCommandBuffers(device_.logical(), command_pool_, 1, &res.buffer);
            }
        });
        command_lists_.clear();
        frame_command_lists_.clear();
        for (auto& bucket : retired_command_lists_)
        {
            bucket.clear();
        }

        for (u32 i = 0; i < kMaxFramesInFlight; ++i)
        {
            if (frame_sync_[i].image_available != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device_.logical(), frame_sync_[i].image_available, nullptr);
            }
            if (frame_sync_[i].in_flight != VK_NULL_HANDLE)
            {
                vkDestroyFence(device_.logical(), frame_sync_[i].in_flight, nullptr);
            }
            frame_sync_[i] = {};
        }

        if (command_pool_ != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device_.logical(), command_pool_, nullptr);
            command_pool_ = VK_NULL_HANDLE;
        }

        swapchain_.shutdown(device_.logical());

        if (vma_allocator_ != nullptr)
        {
            vmaDestroyAllocator(vma_allocator_);
            vma_allocator_ = nullptr;
        }

        if (surface_ != VK_NULL_HANDLE && instance_.handle() != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(instance_.handle(), surface_, nullptr);
            surface_ = VK_NULL_HANDLE;
        }

        device_.shutdown();
        instance_.shutdown();

        window_          = nullptr;
        in_frame_        = false;
        swapchain_dirty_ = false;
        current_frame_   = 0;
        current_image_   = 0;

        ROVER_LOG_INFO("GraphicsDeviceVulkan shut down");
    }

    void GraphicsDeviceVulkan::recreate_swapchain()
    {
        if (window_ == nullptr)
        {
            return;
        }

        vkDeviceWaitIdle(device_.logical());

        unregister_swapchain_textures();

        if (!swapchain_.recreate(device_.physical(),
                                 device_.logical(),
                                 surface_,
                                 device_.queue_families(),
                                 window_->get_width(),
                                 window_->get_height()))
        {
            ROVER_LOG_WARN("Failed to recreate swapchain");
            return;
        }
        register_swapchain_textures();
        swapchain_dirty_ = false;
    }

    void GraphicsDeviceVulkan::register_swapchain_textures()
    {
        const auto& images = swapchain_.images();
        const auto& views  = swapchain_.image_views();
        swapchain_texture_handles_.clear();
        swapchain_texture_handles_.reserve(images.size());

        for (usize i = 0; i < images.size(); ++i)
        {
            VkTextureResource res{};
            res.image             = images[i];
            res.view              = views[i];
            res.allocation        = nullptr;
            res.format            = swapchain_.image_format();
            res.extent            = {swapchain_.extent().width, swapchain_.extent().height, 1};
            res.mip_levels        = 1;
            res.array_layers      = 1;
            res.owns_image        = false;
            const TextureHandle h = textures_.add(res);
            swapchain_texture_handles_.push_back(h);
        }
    }

    void GraphicsDeviceVulkan::unregister_swapchain_textures()
    {
        for (TextureHandle h : swapchain_texture_handles_)
        {
            textures_.remove(h);
        }
        swapchain_texture_handles_.clear();
    }

    const char* GraphicsDeviceVulkan::get_device_name() const
    {
        return device_.device_name();
    }

    const char* GraphicsDeviceVulkan::get_api_name() const
    {
        return api_name_;
    }

} // namespace rover
