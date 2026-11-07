#include "core/log/log.h"
#include "drivers/vulkan/graphics_device_vulkan.h"
#include "drivers/vulkan/vk_sync.h"
#include "drivers/vulkan/vk_texture_state.h"

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
        std::unordered_set<BufferHandle> seen_buffers;
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
        std::vector<TextureBarrier> normalized;
        for (const auto& barrier : textures)
        {
            const auto*             res = textures_.get(barrier.texture);
            TextureSubresourceRange range{};
            if (!res || !texture_supports(*res, barrier.access) ||
                !vk_texture_range(*res, barrier.range, range, device_.separate_depth_stencil_layouts()))
            {
                ROVER_LOG_ERROR("cmd_resource_barriers: invalid texture, usage or subresource range");
                return false;
            }
            for (const auto& previous : normalized)
            {
                if (previous.texture == barrier.texture && texture_ranges_overlap(previous.range, range))
                {
                    ROVER_LOG_ERROR("cmd_resource_barriers: overlapping texture requests in one batch");
                    return false;
                }
            }
            bool initialized = true;
            for_texture_cells(range, [&](auto aspect, auto mip, auto layer) {
                if (resource_access_is_read(barrier.access) &&
                    vk_texture_state(*res, aspect, mip, layer).access == ResourceAccess::Undefined)
                {
                    initialized = false;
                }
            });
            if (!initialized)
            {
                ROVER_LOG_ERROR("cmd_resource_barriers: texture subresource read before initialization");
                return false;
            }
            normalized.push_back({barrier.texture, barrier.access, range});
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
        for (const auto& barrier : normalized)
        {
            const auto& res      = *textures_.get(barrier.texture);
            const bool  separate = device_.separate_depth_stencil_layouts();
            const bool  coupled =
                !separate && texture_aspects(res.engine_format) == (TextureAspect::Depth | TextureAspect::Stencil);
            for_texture_cells(barrier.range, [&](auto aspect, auto mip, auto layer) {
                if (coupled && aspect == TextureAspect::Stencil)
                {
                    return;
                }
                const auto& state = vk_texture_state(res, aspect, mip, layer);
                if (state.access == barrier.access && !resource_access_is_write(state.access))
                {
                    return;
                }
                const auto           after = vk_access_scope(barrier.access);
                VkImageMemoryBarrier b{};
                b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                b.srcAccessMask       = state.sync_access;
                b.dstAccessMask       = after.access;
                b.oldLayout           = vk_texture_layout(res, aspect, state.access, separate);
                b.newLayout           = vk_texture_layout(res, aspect, barrier.access, separate);
                b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.image               = res.image;
                b.subresourceRange    = {
                    vk_texture_aspects(coupled ? TextureAspect::Depth | TextureAspect::Stencil : aspect),
                    mip,
                    1,
                    layer,
                    1};
                image_barriers.push_back(b);
                source_stages |= state.sync_stages;
                target_stages |= after.stages;
            });
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
        for (const auto& barrier : normalized)
        {
            auto& res = *textures_.get(barrier.texture);
            for_texture_cells(barrier.range, [&](auto aspect, auto mip, auto layer) {
                vk_track_access(vk_texture_state(res, aspect, mip, layer), barrier.access);
            });
        }
        return true;
    }

    bool GraphicsDeviceVulkan::cmd_copy_texture_to_buffer(CommandListHandle cmd,
                                                          TextureHandle     texture,
                                                          BufferHandle      buffer,
                                                          TextureCopyRegion region)
    {
        auto*                   cl          = command_lists_.get(cmd);
        const auto*             image       = textures_.get(texture);
        const auto*             destination = buffers_.get(buffer);
        TextureSubresourceRange range{};
        if (!cl || !cl->recording || cl->in_render_pass || cmd != active_command_list_ || !image || !destination ||
            !normalize_texture_range(
                image->engine_format, image->mip_levels, image->array_layers, region.range, range) ||
            range.mip_count != 1 || region.buffer_offset % 4 != 0 ||
            (image->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0 ||
            destination->access != ResourceAccess::TransferWrite)
        {
            ROVER_LOG_ERROR("cmd_copy_texture_to_buffer: invalid copy resources, states or range");
            return false;
        }
        const auto       bytes = texture_plane_bytes(image->engine_format, range.aspects);
        const VkExtent3D extent{texture_mip_extent(image->extent.width, range.base_mip),
                                texture_mip_extent(image->extent.height, range.base_mip),
                                texture_mip_extent(image->extent.depth, range.base_mip)};
        const auto       size =
            static_cast<VkDeviceSize>(extent.width) * extent.height * extent.depth * range.layer_count * bytes;
        if (!bytes || region.buffer_offset > destination->size || size > destination->size - region.buffer_offset)
        {
            ROVER_LOG_ERROR("cmd_copy_texture_to_buffer: unsupported plane or insufficient destination capacity");
            return false;
        }
        bool ready = true;
        for_texture_cells(range, [&](auto aspect, auto mip, auto layer) {
            ready &= vk_texture_state(*image, aspect, mip, layer).access == ResourceAccess::TransferRead;
        });
        if (!ready)
        {
            ROVER_LOG_ERROR("cmd_copy_texture_to_buffer: image range must be TransferRead");
            return false;
        }
        VkBufferImageCopy copy{};
        copy.bufferOffset     = region.buffer_offset;
        copy.imageSubresource = {
            vk_texture_aspects(range.aspects), range.base_mip, range.base_layer, range.layer_count};
        copy.imageExtent = extent;
        vkCmdCopyImageToBuffer(
            cl->buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination->buffer, 1, &copy);
        return true;
    }

    bool GraphicsDeviceVulkan::cmd_clear_texture(CommandListHandle       cmd,
                                                 TextureHandle           texture,
                                                 const ClearValue&       clear,
                                                 TextureSubresourceRange requested)
    {
        auto*                   cl    = command_lists_.get(cmd);
        const auto*             image = textures_.get(texture);
        TextureSubresourceRange range{};
        if (!cl || !cl->recording || cl->in_render_pass || cmd != active_command_list_ || !image ||
            !normalize_texture_range(image->engine_format, image->mip_levels, image->array_layers, requested, range) ||
            (image->usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
        {
            ROVER_LOG_ERROR("cmd_clear_texture: invalid command or range");
            return false;
        }
        bool ready = true;
        for_texture_cells(range, [&](auto aspect, auto mip, auto layer) {
            ready &= vk_texture_state(*image, aspect, mip, layer).access == ResourceAccess::TransferWrite;
        });
        if (!ready)
        {
            ROVER_LOG_ERROR("cmd_clear_texture: image range must be TransferWrite");
            return false;
        }
        const VkImageSubresourceRange native{
            vk_texture_aspects(range.aspects), range.base_mip, range.mip_count, range.base_layer, range.layer_count};
        if (range.aspects == TextureAspect::Color)
        {
            VkClearColorValue value{};
            for (u32 c = 0; c < 4; ++c)
            {
                value.float32[c] = clear.color[c];
            }
            vkCmdClearColorImage(cl->buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &value, 1, &native);
        }
        else
        {
            const VkClearDepthStencilValue value{clear.depth, clear.stencil};
            vkCmdClearDepthStencilImage(
                cl->buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &value, 1, &native);
        }
        return true;
    }
} // namespace rover
