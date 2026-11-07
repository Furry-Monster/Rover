#pragma once

#include "core/graphics/graphics_sync.h"

#include <volk.h>

namespace rover
{

    struct VkAccessScope
    {
        VkPipelineStageFlags stages;
        VkAccessFlags        access;
        VkImageLayout        layout;
    };

    // Shader access is intentionally conservative until per-stage graph
    // declarations are available. Vertex/index/transfer/attachment scopes are
    // precise; all accesses describe the entire resource.
    inline VkAccessScope vk_access_scope(ResourceAccess access)
    {
        switch (access)
        {
            case ResourceAccess::ShaderRead:
                return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            case ResourceAccess::StorageRead:
                return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL};
            case ResourceAccess::StorageWrite:
                return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL};
            case ResourceAccess::ColorAttachment:
                return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            case ResourceAccess::DepthStencilAttachment:
                return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            case ResourceAccess::TransferRead:
                return {
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
            case ResourceAccess::TransferWrite:
                return {
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL};
            case ResourceAccess::VertexRead:
                return {
                    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED};
            case ResourceAccess::IndexRead:
                return {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED};
            case ResourceAccess::UniformRead:
                return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED};
            case ResourceAccess::IndirectRead:
                return {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                        VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                        VK_IMAGE_LAYOUT_UNDEFINED};
            case ResourceAccess::HostRead:
                return {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED};
            case ResourceAccess::HostWrite:
                return {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED};
            case ResourceAccess::Present:
                return {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
            default:
                return {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED};
        }
    }

    // Retain the last writer and all subsequent read scopes. A later reader at
    // a different stage still needs visibility of that writer; a later write
    // must wait for every reader, not just the most recently declared one.
    template <typename Resource>
    void vk_track_access(Resource& resource, ResourceAccess access)
    {
        const auto scope = vk_access_scope(access);
        if (resource_access_is_write(access) || resource.access == ResourceAccess::Undefined)
        {
            resource.sync_stages = scope.stages;
            resource.sync_access = scope.access;
        }
        else
        {
            resource.sync_stages |= scope.stages;
            resource.sync_access |= scope.access;
        }
        resource.access = access;
    }

    inline VkImageAspectFlags vk_image_aspect(VkFormat format)
    {
        switch (format)
        {
            case VK_FORMAT_D32_SFLOAT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

} // namespace rover
