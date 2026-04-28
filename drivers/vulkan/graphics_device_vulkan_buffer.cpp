#include "drivers/vulkan/graphics_device_vulkan.h"

#include "core/log/log.h"
#include "drivers/vulkan/vk_format.h"

#include <cstring>

namespace rover {

namespace {

VmaMemoryUsage to_vma_usage(MemoryUsage m) {
    switch (m) {
        case MemoryUsage::GpuOnly:  return VMA_MEMORY_USAGE_GPU_ONLY;
        case MemoryUsage::CpuToGpu: return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case MemoryUsage::GpuToCpu: return VMA_MEMORY_USAGE_GPU_TO_CPU;
    }
    return VMA_MEMORY_USAGE_AUTO;
}

VkImageAspectFlags aspect_for_format(VkFormat fmt) {
    switch (fmt) {
        case VK_FORMAT_D32_SFLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

} // namespace

BufferHandle GraphicsDeviceVulkan::create_buffer(const BufferDesc& desc) {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size        = desc.size;
    buffer_info.usage       = to_vk_buffer_usage(desc.usage)
                              | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                              | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = to_vma_usage(desc.memory);
    if (desc.memory == MemoryUsage::CpuToGpu || desc.memory == MemoryUsage::GpuToCpu) {
        alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VkBufferResource res{};
    res.size   = desc.size;
    res.usage  = desc.usage;
    res.memory = desc.memory;

    const VkResult result = vmaCreateBuffer(vma_allocator_, &buffer_info, &alloc_info,
                                            &res.buffer, &res.allocation, nullptr);
    if (result != VK_SUCCESS) {
        ROVER_LOG_ERROR("vmaCreateBuffer failed: {}", static_cast<i32>(result));
        return INVALID_HANDLE;
    }
    return buffers_.add(res);
}

void GraphicsDeviceVulkan::destroy_buffer(BufferHandle handle) {
    VkBufferResource res{};
    if (!buffers_.remove(handle, &res)) return;
    if (res.mapped && res.allocation != nullptr) {
        vmaUnmapMemory(vma_allocator_, res.allocation);
    }
    if (res.buffer != VK_NULL_HANDLE && res.allocation != nullptr) {
        vmaDestroyBuffer(vma_allocator_, res.buffer, res.allocation);
    }
}

void* GraphicsDeviceVulkan::map_buffer(BufferHandle handle) {
    auto* res = buffers_.get(handle);
    if (res == nullptr) return nullptr;
    if (res->mapped) return res->mapped_ptr;
    void* ptr = nullptr;
    if (vmaMapMemory(vma_allocator_, res->allocation, &ptr) != VK_SUCCESS) {
        return nullptr;
    }
    res->mapped     = true;
    res->mapped_ptr = ptr;
    return ptr;
}

void GraphicsDeviceVulkan::unmap_buffer(BufferHandle handle) {
    auto* res = buffers_.get(handle);
    if (res == nullptr || !res->mapped) return;
    vmaUnmapMemory(vma_allocator_, res->allocation);
    res->mapped     = false;
    res->mapped_ptr = nullptr;
}

void GraphicsDeviceVulkan::update_buffer(BufferHandle handle, const void* data,
                                         usize size, usize offset) {
    auto* res = buffers_.get(handle);
    if (res == nullptr || data == nullptr || size == 0) return;
    if (res->memory == MemoryUsage::GpuOnly) {
        ROVER_LOG_WARN("update_buffer on GpuOnly buffer not implemented (staging path TODO)");
        return;
    }
    void* ptr = nullptr;
    if (vmaMapMemory(vma_allocator_, res->allocation, &ptr) != VK_SUCCESS) {
        return;
    }
    std::memcpy(static_cast<u8*>(ptr) + offset, data, size);
    vmaFlushAllocation(vma_allocator_, res->allocation, offset, size);
    vmaUnmapMemory(vma_allocator_, res->allocation);
}

TextureHandle GraphicsDeviceVulkan::create_texture(const TextureDesc& desc) {
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
    if (desc.type == TextureType::TextureCube) {
        image_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkTextureResource res{};
    res.format       = image_info.format;
    res.extent       = image_info.extent;
    res.mip_levels   = desc.mip_levels;
    res.array_layers = image_info.arrayLayers;
    res.owns_image   = true;

    const VkResult result = vmaCreateImage(vma_allocator_, &image_info, &alloc_info,
                                           &res.image, &res.allocation, nullptr);
    if (result != VK_SUCCESS) {
        ROVER_LOG_ERROR("vmaCreateImage failed: {}", static_cast<i32>(result));
        return INVALID_HANDLE;
    }

    VkImageViewCreateInfo view_info{};
    view_info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image    = res.image;
    view_info.viewType = to_vk_image_view_type(desc.type);
    view_info.format   = res.format;
    view_info.subresourceRange.aspectMask     = aspect_for_format(res.format);
    view_info.subresourceRange.baseMipLevel   = 0;
    view_info.subresourceRange.levelCount     = res.mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount     = res.array_layers;

    if (vkCreateImageView(device_.logical(), &view_info, nullptr, &res.view) != VK_SUCCESS) {
        ROVER_LOG_ERROR("vkCreateImageView failed");
        vmaDestroyImage(vma_allocator_, res.image, res.allocation);
        return INVALID_HANDLE;
    }

    return textures_.add(res);
}

void GraphicsDeviceVulkan::destroy_texture(TextureHandle handle) {
    VkTextureResource res{};
    if (!textures_.remove(handle, &res)) return;
    // Swapchain-owned textures are managed by VkSwapchainWrapper; the user
    // should not destroy them through this API. We silently no-op the GPU
    // teardown to avoid double-free, but still remove the pool entry so the
    // handle is reusable.
    if (!res.owns_image) {
        return;
    }
    if (res.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_.logical(), res.view, nullptr);
    }
    if (res.image != VK_NULL_HANDLE && res.allocation != nullptr) {
        vmaDestroyImage(vma_allocator_, res.image, res.allocation);
    }
}

void GraphicsDeviceVulkan::update_texture(TextureHandle handle, const void* data, usize size) {
    (void)handle;
    (void)data;
    (void)size;
    ROVER_LOG_WARN("update_texture not implemented yet");
}

SamplerHandle GraphicsDeviceVulkan::create_sampler(const SamplerDesc& desc) {
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
    if (vkCreateSampler(device_.logical(), &info, nullptr, &sampler) != VK_SUCCESS) {
        ROVER_LOG_ERROR("vkCreateSampler failed");
        return INVALID_HANDLE;
    }
    return samplers_.add(sampler);
}

void GraphicsDeviceVulkan::destroy_sampler(SamplerHandle handle) {
    VkSampler sampler = VK_NULL_HANDLE;
    if (!samplers_.remove(handle, &sampler)) return;
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device_.logical(), sampler, nullptr);
    }
}

} // namespace rover
