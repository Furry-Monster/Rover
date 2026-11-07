#include "core/log/log.h"
#include "drivers/vulkan/graphics_device_vulkan.h"
#include "drivers/vulkan/vk_format.h"
#include "drivers/vulkan/vk_sync.h"
#include "drivers/vulkan/vk_texture_state.h"

#include <cstring>

namespace rover
{

    namespace
    {

        VmaMemoryUsage to_vma_usage(MemoryUsage m)
        {
            switch (m)
            {
                case MemoryUsage::GpuOnly:
                    return VMA_MEMORY_USAGE_GPU_ONLY;
                case MemoryUsage::CpuToGpu:
                    return VMA_MEMORY_USAGE_CPU_TO_GPU;
                case MemoryUsage::GpuToCpu:
                    return VMA_MEMORY_USAGE_GPU_TO_CPU;
            }
            return VMA_MEMORY_USAGE_AUTO;
        }

    } // namespace

    BufferHandle GraphicsDeviceVulkan::create_buffer(const BufferDesc& desc)
    {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size  = desc.size;
        buffer_info.usage =
            to_vk_buffer_usage(desc.usage) | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = to_vma_usage(desc.memory);
        if (desc.memory == MemoryUsage::CpuToGpu || desc.memory == MemoryUsage::GpuToCpu)
        {
            alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }

        VkBufferResource res{};
        res.size   = desc.size;
        res.usage  = desc.usage;
        res.memory = desc.memory;

        const VkResult result =
            vmaCreateBuffer(vma_allocator_, &buffer_info, &alloc_info, &res.buffer, &res.allocation, nullptr);
        if (result != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vmaCreateBuffer failed: {}", static_cast<i32>(result));
            return INVALID_HANDLE;
        }
        return buffers_.add(res);
    }

    void GraphicsDeviceVulkan::destroy_buffer(BufferHandle handle)
    {
        VkBufferResource res{};
        if (!buffers_.remove(handle, &res))
        {
            return;
        }
        if (res.mapped && res.allocation != nullptr)
        {
            vmaUnmapMemory(vma_allocator_, res.allocation);
        }
        if (res.buffer != VK_NULL_HANDLE && res.allocation != nullptr)
        {
            vmaDestroyBuffer(vma_allocator_, res.buffer, res.allocation);
        }
    }

    void* GraphicsDeviceVulkan::map_buffer(BufferHandle handle)
    {
        auto* res = buffers_.get(handle);
        if (res == nullptr)
        {
            return nullptr;
        }
        if (res->mapped)
        {
            return res->mapped_ptr;
        }
        void* ptr = nullptr;
        if (vmaMapMemory(vma_allocator_, res->allocation, &ptr) != VK_SUCCESS)
        {
            return nullptr;
        }
        res->mapped     = true;
        res->mapped_ptr = ptr;
        if (res->memory == MemoryUsage::GpuToCpu)
        {
            // Caller must first wait for GPU completion; invalidation only makes
            // non-coherent device writes visible to the CPU cache.
            vmaInvalidateAllocation(vma_allocator_, res->allocation, 0, VK_WHOLE_SIZE);
        }
        return ptr;
    }

    void GraphicsDeviceVulkan::unmap_buffer(BufferHandle handle)
    {
        auto* res = buffers_.get(handle);
        if (res == nullptr || !res->mapped)
        {
            return;
        }
        vmaUnmapMemory(vma_allocator_, res->allocation);
        res->mapped     = false;
        res->mapped_ptr = nullptr;
    }

    namespace
    {

        // Allocates a one-shot command buffer, executes the recorder lambda inside
        // it, then submits to the graphics queue and waits for completion. Used
        // for staging uploads (Phase 2 simple impl; will move to async copies later).
        template <typename Recorder>
        bool one_shot_submit(VkDevice dev, VkCommandPool pool, VkQueue queue, Recorder&& recorder)
        {
            VkCommandBufferAllocateInfo alloc{};
            alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            alloc.commandPool        = pool;
            alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            alloc.commandBufferCount = 1;

            VkCommandBuffer cmd = VK_NULL_HANDLE;
            if (vkAllocateCommandBuffers(dev, &alloc, &cmd) != VK_SUCCESS)
            {
                return false;
            }

            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS)
            {
                vkFreeCommandBuffers(dev, pool, 1, &cmd);
                return false;
            }

            recorder(cmd);

            if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
            {
                vkFreeCommandBuffers(dev, pool, 1, &cmd);
                return false;
            }

            VkSubmitInfo submit{};
            submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers    = &cmd;
            if (vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
            {
                vkFreeCommandBuffers(dev, pool, 1, &cmd);
                return false;
            }
            vkQueueWaitIdle(queue);
            vkFreeCommandBuffers(dev, pool, 1, &cmd);
            return true;
        }

    } // namespace

    void GraphicsDeviceVulkan::update_buffer(BufferHandle handle, const void* data, usize size, usize offset)
    {
        auto* res = buffers_.get(handle);
        if (res == nullptr || data == nullptr || size == 0 || offset > res->size || size > res->size - offset)
        {
            return;
        }
        if (!frame_command_lists_.empty())
        {
            ROVER_LOG_ERROR("update_buffer: upload must precede this frame's command recording");
            return;
        }

        if (res->memory != MemoryUsage::GpuOnly)
        {
            // CPU-mappable: write directly.
            void* ptr = nullptr;
            if (vmaMapMemory(vma_allocator_, res->allocation, &ptr) != VK_SUCCESS)
            {
                return;
            }
            std::memcpy(static_cast<u8*>(ptr) + offset, data, size);
            vmaFlushAllocation(vma_allocator_, res->allocation, offset, size);
            vmaUnmapMemory(vma_allocator_, res->allocation);
            vk_track_access(*res, ResourceAccess::HostWrite);
            return;
        }

        // GpuOnly: stage through a host-visible buffer + vkCmdCopyBuffer.
        VkBufferCreateInfo staging_info{};
        staging_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        staging_info.size        = size;
        staging_info.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staging_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo staging_alloc{};
        staging_alloc.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        staging_alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer          staging_buf = VK_NULL_HANDLE;
        VmaAllocation     staging_mem = nullptr;
        VmaAllocationInfo info{};
        if (vmaCreateBuffer(vma_allocator_, &staging_info, &staging_alloc, &staging_buf, &staging_mem, &info) !=
            VK_SUCCESS)
        {
            ROVER_LOG_ERROR("update_buffer: failed to allocate staging buffer");
            return;
        }
        std::memcpy(info.pMappedData, data, size);
        vmaFlushAllocation(vma_allocator_, staging_mem, 0, size);

        ResourceAccess target = ResourceAccess::TransferWrite;
        if ((res->usage & BufferUsage::Vertex) != static_cast<BufferUsage>(0))
        {
            target = ResourceAccess::VertexRead;
        }
        else if ((res->usage & BufferUsage::Index) != static_cast<BufferUsage>(0))
        {
            target = ResourceAccess::IndexRead;
        }
        else if ((res->usage & BufferUsage::Uniform) != static_cast<BufferUsage>(0))
        {
            target = ResourceAccess::UniformRead;
        }
        else if ((res->usage & BufferUsage::Storage) != static_cast<BufferUsage>(0))
        {
            target = ResourceAccess::StorageRead;
        }

        const bool copied =
            one_shot_submit(device_.logical(), command_pool_, device_.graphics_queue(), [&](VkCommandBuffer cmd) {
                VkBufferMemoryBarrier barrier{};
                barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                barrier.srcAccessMask       = res->sync_access;
                barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer              = res->buffer;
                barrier.size                = VK_WHOLE_SIZE;
                vkCmdPipelineBarrier(
                    cmd, res->sync_stages, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
                VkBufferCopy region{};
                region.srcOffset = 0;
                region.dstOffset = offset;
                region.size      = size;
                vkCmdCopyBuffer(cmd, staging_buf, res->buffer, 1, &region);
                if (target != ResourceAccess::TransferWrite)
                {
                    const auto destination = vk_access_scope(target);
                    barrier.srcAccessMask  = VK_ACCESS_TRANSFER_WRITE_BIT;
                    barrier.dstAccessMask  = destination.access;
                    vkCmdPipelineBarrier(cmd,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         destination.stages,
                                         0,
                                         0,
                                         nullptr,
                                         1,
                                         &barrier,
                                         0,
                                         nullptr);
                }
            });
        if (!copied)
        {
            ROVER_LOG_ERROR("update_buffer: staging copy submit failed");
        }
        else
        {
            vk_track_access(*res, ResourceAccess::TransferWrite);
            if (target != ResourceAccess::TransferWrite)
            {
                vk_track_access(*res, target);
            }
        }

        vmaDestroyBuffer(vma_allocator_, staging_buf, staging_mem);
    }

    TextureHandle GraphicsDeviceVulkan::create_texture(const TextureDesc& desc)
    {
        VkImageCreateInfo image_info{};
        image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType     = to_vk_image_type(desc.type);
        image_info.format        = to_vk_format(desc.format);
        image_info.extent        = {desc.width, desc.height, desc.depth};
        image_info.mipLevels     = desc.mip_levels;
        image_info.arrayLayers   = desc.type == TextureType::TextureCube ? 6 * desc.array_layers : desc.array_layers;
        image_info.samples       = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage         = to_vk_image_usage(desc.usage) | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (desc.type == TextureType::TextureCube)
        {
            image_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkTextureResource res{};
        res.format        = image_info.format;
        res.engine_format = desc.format;
        res.type          = desc.type;
        res.extent        = image_info.extent;
        res.mip_levels    = desc.mip_levels;
        res.array_layers  = image_info.arrayLayers;
        res.owns_image    = true;
        res.usage         = image_info.usage;

        const VkResult result =
            vmaCreateImage(vma_allocator_, &image_info, &alloc_info, &res.image, &res.allocation, nullptr);
        if (result != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vmaCreateImage failed: {}", static_cast<i32>(result));
            return INVALID_HANDLE;
        }

        VkImageViewCreateInfo view_info{};
        view_info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image    = res.image;
        view_info.viewType = to_vk_image_view_type(desc.type);
        if (desc.type == TextureType::TextureCube && res.array_layers > 6)
        {
            view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        }
        if (desc.type == TextureType::Texture2D && res.array_layers > 1)
        {
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
        res.view_type                             = view_info.viewType;
        view_info.format                          = res.format;
        view_info.subresourceRange.aspectMask     = vk_image_aspect(res.format);
        view_info.subresourceRange.baseMipLevel   = 0;
        view_info.subresourceRange.levelCount     = res.mip_levels;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount     = res.array_layers;

        if (vkCreateImageView(device_.logical(), &view_info, nullptr, &res.view) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vkCreateImageView failed");
            vmaDestroyImage(vma_allocator_, res.image, res.allocation);
            return INVALID_HANDLE;
        }

        vk_initialize_texture_states(res);
        return textures_.add(res);
    }

    void GraphicsDeviceVulkan::destroy_texture(TextureHandle handle)
    {
        VkTextureResource res{};
        if (!textures_.remove(handle, &res))
        {
            return;
        }
        // Swapchain-owned textures are managed by VkSwapchainWrapper; the user
        // should not destroy them through this API. We silently no-op the GPU
        // teardown to avoid double-free, but still remove the pool entry so the
        // handle is reusable.
        if (!res.owns_image)
        {
            return;
        }
        for (const auto& view : res.views)
        {
            vkDestroyImageView(device_.logical(), view.view, nullptr);
        }
        if (res.view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device_.logical(), res.view, nullptr);
        }
        if (res.image != VK_NULL_HANDLE && res.allocation != nullptr)
        {
            vmaDestroyImage(vma_allocator_, res.image, res.allocation);
        }
    }

    void GraphicsDeviceVulkan::update_texture(TextureHandle           handle,
                                              const void*             data,
                                              usize                   size,
                                              TextureSubresourceRange requested)
    {
        auto*                   image = textures_.get(handle);
        TextureSubresourceRange range{};
        const bool              separate = device_.separate_depth_stencil_layouts();
        if (!image || !data || !image->owns_image || !frame_command_lists_.empty() ||
            !vk_texture_range(*image, requested, range, separate) || range.mip_count != 1)
        {
            ROVER_LOG_ERROR("update_texture: invalid upload range or frame recording has begun");
            return;
        }
        const auto       bytes = texture_plane_bytes(image->engine_format, range.aspects);
        const VkExtent3D extent{texture_mip_extent(image->extent.width, range.base_mip),
                                texture_mip_extent(image->extent.height, range.base_mip),
                                texture_mip_extent(image->extent.depth, range.base_mip)};
        const u64 required = static_cast<u64>(extent.width) * extent.height * extent.depth * range.layer_count * bytes;
        if (!bytes || required != size)
        {
            ROVER_LOG_ERROR("update_texture: upload size must match the selected tightly packed plane");
            return;
        }
        VkBufferCreateInfo info{};
        info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size        = size;
        info.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocation{};
        allocation.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        allocation.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VkBuffer          staging = VK_NULL_HANDLE;
        VmaAllocation     memory  = nullptr;
        VmaAllocationInfo mapped{};
        if (vmaCreateBuffer(vma_allocator_, &info, &allocation, &staging, &memory, &mapped) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("update_texture: failed to allocate staging buffer");
            return;
        }
        std::memcpy(mapped.pMappedData, data, size);
        vmaFlushAllocation(vma_allocator_, memory, 0, size);
        const bool sampled = (image->usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0;
        const bool copied =
            one_shot_submit(device_.logical(), command_pool_, device_.graphics_queue(), [&](VkCommandBuffer cmd) {
                std::vector<VkImageMemoryBarrier> barriers;
                VkPipelineStageFlags              source = 0;
                for_texture_cells(range, [&](auto aspect, auto mip, auto layer) {
                    const auto&          state = vk_texture_state(*image, aspect, mip, layer);
                    VkImageMemoryBarrier b{};
                    b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    b.srcAccessMask       = state.sync_access;
                    b.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
                    b.oldLayout           = vk_texture_layout(*image, aspect, state.access, separate);
                    b.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    b.image               = image->image;
                    b.subresourceRange    = {vk_texture_aspects(aspect), mip, 1, layer, 1};
                    barriers.push_back(b);
                    source |= state.sync_stages;
                });
                vkCmdPipelineBarrier(cmd,
                                     source,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     static_cast<u32>(barriers.size()),
                                     barriers.data());
                VkBufferImageCopy copy{};
                copy.imageSubresource = {
                    vk_texture_aspects(range.aspects), range.base_mip, range.base_layer, range.layer_count};
                copy.imageExtent = extent;
                vkCmdCopyBufferToImage(cmd, staging, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
                if (sampled)
                {
                    for (auto& b : barriers)
                    {
                        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                        b.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                        b.newLayout = vk_texture_layout(*image, range.aspects, ResourceAccess::ShaderRead, separate);
                    }
                    vkCmdPipelineBarrier(cmd,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                         0,
                                         0,
                                         nullptr,
                                         0,
                                         nullptr,
                                         static_cast<u32>(barriers.size()),
                                         barriers.data());
                }
            });
        if (copied)
        {
            for_texture_cells(range, [&](auto aspect, auto mip, auto layer) {
                auto& state = vk_texture_state(*image, aspect, mip, layer);
                vk_track_access(state, ResourceAccess::TransferWrite);
                if (sampled)
                {
                    vk_track_access(state, ResourceAccess::ShaderRead);
                }
            });
        }
        else
        {
            ROVER_LOG_ERROR("update_texture: staging copy submit failed");
        }
        vmaDestroyBuffer(vma_allocator_, staging, memory);
    }

    VkImageView GraphicsDeviceVulkan::texture_view(TextureHandle           handle,
                                                   TextureSubresourceRange requested,
                                                   bool                    attachment)
    {
        auto*                   image = textures_.get(handle);
        TextureSubresourceRange range{};
        if (!image ||
            !normalize_texture_range(image->engine_format, image->mip_levels, image->array_layers, requested, range) ||
            (attachment && (range.mip_count != 1 || range.layer_count != 1 || image->type == TextureType::Texture3D)))
        {
            ROVER_LOG_ERROR("texture_view: invalid subresource range");
            return VK_NULL_HANDLE;
        }
        VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
        if (image->type == TextureType::Texture3D)
        {
            type = VK_IMAGE_VIEW_TYPE_3D;
        }
        else if (!attachment && image->type == TextureType::TextureCube && range.base_layer % 6 == 0 &&
                 range.layer_count % 6 == 0)
        {
            type = range.layer_count == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        }
        else if (range.layer_count > 1)
        {
            type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
        TextureSubresourceRange full{};
        normalize_texture_range(image->engine_format, image->mip_levels, image->array_layers, {}, full);
        if (range == full && type == image->view_type)
        {
            return image->view;
        }
        for (const auto& cached : image->views)
        {
            if (cached.range == range)
            {
                return cached.view;
            }
        }
        VkImageViewCreateInfo info{};
        info.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image            = image->image;
        info.viewType         = type;
        info.format           = image->format;
        info.subresourceRange = {
            vk_texture_aspects(range.aspects), range.base_mip, range.mip_count, range.base_layer, range.layer_count};
        VkImageView view = VK_NULL_HANDLE;
        if (vkCreateImageView(device_.logical(), &info, nullptr, &view) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("texture_view: vkCreateImageView failed");
            return VK_NULL_HANDLE;
        }
        image->views.push_back({range, view});
        return view;
    }

    SamplerHandle GraphicsDeviceVulkan::create_sampler(const SamplerDesc& desc)
    {
        VkSamplerCreateInfo info{};
        info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter               = to_vk_filter(desc.mag_filter);
        info.minFilter               = to_vk_filter(desc.min_filter);
        info.addressModeU            = to_vk_address_mode(desc.address_u);
        info.addressModeV            = to_vk_address_mode(desc.address_v);
        info.addressModeW            = to_vk_address_mode(desc.address_w);
        info.anisotropyEnable        = desc.max_anisotropy > 1.0f ? VK_TRUE : VK_FALSE;
        info.maxAnisotropy           = desc.max_anisotropy;
        info.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        info.unnormalizedCoordinates = VK_FALSE;
        info.compareEnable           = VK_FALSE;
        info.compareOp               = VK_COMPARE_OP_ALWAYS;
        info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.mipLodBias              = 0.0f;
        info.minLod                  = 0.0f;
        info.maxLod                  = VK_LOD_CLAMP_NONE;

        VkSampler sampler = VK_NULL_HANDLE;
        if (vkCreateSampler(device_.logical(), &info, nullptr, &sampler) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vkCreateSampler failed");
            return INVALID_HANDLE;
        }
        return samplers_.add(sampler);
    }

    void GraphicsDeviceVulkan::destroy_sampler(SamplerHandle handle)
    {
        VkSampler sampler = VK_NULL_HANDLE;
        if (!samplers_.remove(handle, &sampler))
        {
            return;
        }
        if (sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(device_.logical(), sampler, nullptr);
        }
    }

} // namespace rover
