#pragma once

#include "drivers/vulkan/graphics_device_vulkan.h"
#include "drivers/vulkan/vk_sync.h"

namespace rover
{
    inline VkImageAspectFlags vk_texture_aspects(TextureAspect aspects)
    {
        VkImageAspectFlags result = 0;
        if ((aspects & TextureAspect::Color) == TextureAspect::Color)
        {
            result |= VK_IMAGE_ASPECT_COLOR_BIT;
        }
        if ((aspects & TextureAspect::Depth) == TextureAspect::Depth)
        {
            result |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        if ((aspects & TextureAspect::Stencil) == TextureAspect::Stencil)
        {
            result |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        return result;
    }

    inline bool vk_texture_range(const VkTextureResource& image,
                                 TextureSubresourceRange  input,
                                 TextureSubresourceRange& normalized,
                                 bool                     separate)
    {
        if (!normalize_texture_range(image.engine_format, image.mip_levels, image.array_layers, input, normalized))
        {
            return false;
        }
        const auto depth_stencil = TextureAspect::Depth | TextureAspect::Stencil;
        return separate || texture_aspects(image.engine_format) != depth_stencil || normalized.aspects == depth_stencil;
    }

    template <typename Image>
    auto& vk_texture_state(Image& image, TextureAspect aspect, u32 mip, u32 layer)
    {
        return image.states[texture_cell_index(aspect, mip, layer, image.mip_levels, image.array_layers)];
    }

    inline VkImageLayout vk_texture_layout(const VkTextureResource& image,
                                           TextureAspect            aspect,
                                           ResourceAccess           access,
                                           bool                     separate)
    {
        if (separate && texture_aspects(image.engine_format) == (TextureAspect::Depth | TextureAspect::Stencil))
        {
            if (access == ResourceAccess::DepthStencilAttachment)
            {
                return aspect == TextureAspect::Depth ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
                                                      : VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
            }
            if (access == ResourceAccess::ShaderRead)
            {
                return aspect == TextureAspect::Depth ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
                                                      : VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
            }
        }
        return vk_access_scope(access).layout;
    }

    inline void vk_initialize_texture_states(VkTextureResource& image)
    {
        image.states.resize(static_cast<usize>(3) * image.mip_levels * image.array_layers);
    }
} // namespace rover
