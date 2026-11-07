#include "core/log/log.h"
#include "drivers/vulkan/graphics_device_vulkan.h"
#include "drivers/vulkan/vk_sync.h"

#include <unordered_set>
#include <vector>

namespace rover
{

    namespace
    {
        bool buffer_supports(const VkBufferResource& buffer, ResourceAccess access)
        {
            if (!resource_access_supports_buffer(access))
            {
                return false;
            }
            auto has = [&](BufferUsage usage) {
                return (buffer.usage & usage) == usage;
            };
            switch (access)
            {
                case ResourceAccess::ShaderRead:
                case ResourceAccess::StorageRead:
                case ResourceAccess::StorageWrite:
                    return has(BufferUsage::Storage);
                case ResourceAccess::UniformRead:
                    return has(BufferUsage::Uniform);
                case ResourceAccess::VertexRead:
                    return has(BufferUsage::Vertex);
                case ResourceAccess::IndexRead:
                    return has(BufferUsage::Index);
                case ResourceAccess::IndirectRead:
                    return has(BufferUsage::Indirect);
                case ResourceAccess::HostRead:
                case ResourceAccess::HostWrite:
                    return buffer.memory != MemoryUsage::GpuOnly;
                default:
                    // Vulkan buffers always include transfer source/destination.
                    return true;
            }
        }

        bool texture_supports(const VkTextureResource& texture, ResourceAccess access)
        {
            if (!resource_access_supports_texture(access))
            {
                return false;
            }
            const bool depth = vk_image_aspect(texture.format) != VK_IMAGE_ASPECT_COLOR_BIT;
            switch (access)
            {
                case ResourceAccess::ShaderRead:
                    return (texture.usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0;
                case ResourceAccess::StorageRead:
                case ResourceAccess::StorageWrite:
                    return (texture.usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
                case ResourceAccess::ColorAttachment:
                    return !depth && (texture.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0;
                case ResourceAccess::DepthStencilAttachment:
                    return depth && (texture.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
                case ResourceAccess::TransferRead:
                    return (texture.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
                case ResourceAccess::TransferWrite:
                    return (texture.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0;
                case ResourceAccess::Present:
                    return !texture.owns_image;
                default:
                    return false;
            }
        }
    } // namespace

    bool GraphicsDeviceVulkan::cmd_resource_barriers(CommandListHandle               cmd,
                                                     std::span<const BufferBarrier>  buffers,
                                                     std::span<const TextureBarrier> textures)
    {
        auto* cl = command_lists_.get(cmd);
        if (cl == nullptr || !cl->recording || cl->in_render_pass || cmd != active_command_list_)
        {
            ROVER_LOG_ERROR("cmd_resource_barriers: requires an active command list outside a render pass");
            return false;
        }

        // Validate every request before recording or changing any tracked state.
        std::unordered_set<BufferHandle>  seen_buffers;
        std::unordered_set<TextureHandle> seen_textures;
        for (const auto& barrier : buffers)
        {
            const auto* res = buffers_.get(barrier.buffer);
            if (res == nullptr || !seen_buffers.insert(barrier.buffer).second ||
                !buffer_supports(*res, barrier.access) ||
                (res->access == ResourceAccess::Undefined && resource_access_is_read(barrier.access)))
            {
                ROVER_LOG_ERROR("cmd_resource_barriers: invalid/uninitialized buffer {} or access {}",
                                barrier.buffer,
                                static_cast<u32>(barrier.access));
                return false;
            }
        }
        for (const auto& barrier : textures)
        {
            const auto* res = textures_.get(barrier.texture);
            if (res == nullptr || !seen_textures.insert(barrier.texture).second ||
                !texture_supports(*res, barrier.access) ||
                (res->access == ResourceAccess::Undefined && resource_access_is_read(barrier.access)))
            {
                ROVER_LOG_ERROR("cmd_resource_barriers: invalid/uninitialized texture {} or access {}",
                                barrier.texture,
                                static_cast<u32>(barrier.access));
                return false;
            }
        }

        std::vector<VkBufferMemoryBarrier> buffer_barriers;
        std::vector<VkImageMemoryBarrier>  image_barriers;
        VkPipelineStageFlags               source_stages = 0;
        VkPipelineStageFlags               target_stages = 0;
        for (const auto& barrier : buffers)
        {
            const auto& res = *buffers_.get(barrier.buffer);
            if (res.access == barrier.access && !resource_access_is_write(res.access))
            {
                continue;
            }
            const auto            after = vk_access_scope(barrier.access);
            VkBufferMemoryBarrier b{};
            b.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            b.srcAccessMask       = res.sync_access;
            b.dstAccessMask       = after.access;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.buffer              = res.buffer;
            b.offset              = 0;
            b.size                = VK_WHOLE_SIZE;
            buffer_barriers.push_back(b);
            source_stages |= res.sync_stages;
            target_stages |= after.stages;
        }
        for (const auto& barrier : textures)
        {
            const auto& res = *textures_.get(barrier.texture);
            if (res.access == barrier.access && !resource_access_is_write(res.access))
            {
                continue;
            }
            const auto           before = vk_access_scope(res.access);
            const auto           after  = vk_access_scope(barrier.access);
            VkImageMemoryBarrier b{};
            b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.srcAccessMask       = res.sync_access;
            b.dstAccessMask       = after.access;
            b.oldLayout           = before.layout;
            b.newLayout           = after.layout;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image               = res.image;
            b.subresourceRange    = {vk_image_aspect(res.format), 0, res.mip_levels, 0, res.array_layers};
            image_barriers.push_back(b);
            source_stages |= res.sync_stages;
            target_stages |= after.stages;
        }
        if (!buffer_barriers.empty() || !image_barriers.empty())
        {
            vkCmdPipelineBarrier(cl->buffer,
                                 source_stages,
                                 target_stages,
                                 0,
                                 0,
                                 nullptr,
                                 static_cast<u32>(buffer_barriers.size()),
                                 buffer_barriers.data(),
                                 static_cast<u32>(image_barriers.size()),
                                 image_barriers.data());
        }
        for (const auto& barrier : buffers)
        {
            vk_track_access(*buffers_.get(barrier.buffer), barrier.access);
        }
        for (const auto& barrier : textures)
        {
            vk_track_access(*textures_.get(barrier.texture), barrier.access);
        }
        return true;
    }

    bool GraphicsDeviceVulkan::cmd_copy_texture_to_buffer(CommandListHandle cmd,
                                                          TextureHandle     texture,
                                                          BufferHandle      buffer)
    {
        auto*       cl          = command_lists_.get(cmd);
        const auto* image       = textures_.get(texture);
        const auto* destination = buffers_.get(buffer);
        if (cl == nullptr || !cl->recording || cl->in_render_pass || cmd != active_command_list_ || image == nullptr ||
            destination == nullptr)
        {
            ROVER_LOG_ERROR("cmd_copy_texture_to_buffer: invalid command list or resources");
            return false;
        }
        const auto size = static_cast<VkDeviceSize>(image->extent.width) * image->extent.height * 4;
        if ((image->format != VK_FORMAT_R8G8B8A8_UNORM && image->format != VK_FORMAT_R8G8B8A8_SRGB) ||
            image->extent.depth != 1 || image->mip_levels != 1 || image->array_layers != 1 ||
            destination->size < size || image->access != ResourceAccess::TransferRead ||
            destination->access != ResourceAccess::TransferWrite ||
            (image->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0)
        {
            ROVER_LOG_ERROR(
                "cmd_copy_texture_to_buffer: requires RGBA8 2D transfer states and sufficient buffer capacity");
            return false;
        }
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent      = image->extent;
        vkCmdCopyImageToBuffer(
            cl->buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination->buffer, 1, &region);
        return true;
    }

} // namespace rover
