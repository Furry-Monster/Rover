#include "core/log/log.h"
#include "drivers/vulkan/graphics_device_vulkan.h"
#include "drivers/vulkan/vk_format.h"
#include "drivers/vulkan/vk_texture_state.h"

#include <vector>

namespace rover
{

    ShaderHandle GraphicsDeviceVulkan::create_shader(const ShaderDesc& desc)
    {
        if (desc.bytecode == nullptr || desc.bytecode_size == 0)
        {
            ROVER_LOG_ERROR("create_shader: empty bytecode");
            return INVALID_HANDLE;
        }
        if ((desc.bytecode_size % 4) != 0)
        {
            ROVER_LOG_ERROR("create_shader: bytecode_size must be a multiple of 4 (SPIR-V words)");
            return INVALID_HANDLE;
        }

        VkShaderModuleCreateInfo info{};
        info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = desc.bytecode_size;
        info.pCode    = reinterpret_cast<const u32*>(desc.bytecode);

        VkShaderResource res{};
        res.stage = to_vk_shader_stage(desc.stage);
        if (vkCreateShaderModule(device_.logical(), &info, nullptr, &res.module) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vkCreateShaderModule failed");
            return INVALID_HANDLE;
        }
        return shaders_.add(res);
    }

    void GraphicsDeviceVulkan::destroy_shader(ShaderHandle handle)
    {
        VkShaderResource res{};
        if (!shaders_.remove(handle, &res))
        {
            return;
        }
        if (res.module != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device_.logical(), res.module, nullptr);
        }
    }

    RenderPassHandle GraphicsDeviceVulkan::create_render_pass(const RenderPassDesc& desc)
    {
        std::vector<VkAttachmentDescription> attachments;
        attachments.reserve(desc.color_attachments.size() + (desc.has_depth_stencil ? 1u : 0u));

        std::vector<VkAttachmentReference> color_refs;
        color_refs.reserve(desc.color_attachments.size());

        for (usize i = 0; i < desc.color_attachments.size(); ++i)
        {
            const auto&             a = desc.color_attachments[i];
            VkAttachmentDescription att{};
            att.format         = to_vk_format(a.format);
            att.samples        = VK_SAMPLE_COUNT_1_BIT;
            att.loadOp         = to_vk_load_op(a.load_op);
            att.storeOp        = to_vk_store_op(a.store_op);
            att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            att.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            att.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachments.push_back(att);

            VkAttachmentReference ref{};
            ref.attachment = static_cast<u32>(i);
            ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color_refs.push_back(ref);
        }

        VkAttachmentReference depth_ref{};
        if (desc.has_depth_stencil)
        {
            VkAttachmentDescription att{};
            att.format         = to_vk_format(desc.depth_stencil.format);
            att.samples        = VK_SAMPLE_COUNT_1_BIT;
            att.loadOp         = to_vk_load_op(desc.depth_stencil.load_op);
            att.storeOp        = to_vk_store_op(desc.depth_stencil.store_op);
            att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            att.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            att.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.push_back(att);

            depth_ref.attachment = static_cast<u32>(attachments.size() - 1);
            depth_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = static_cast<u32>(color_refs.size());
        subpass.pColorAttachments       = color_refs.empty() ? nullptr : color_refs.data();
        subpass.pDepthStencilAttachment = desc.has_depth_stencil ? &depth_ref : nullptr;

        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info{};
        info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = static_cast<u32>(attachments.size());
        info.pAttachments    = attachments.empty() ? nullptr : attachments.data();
        info.subpassCount    = 1;
        info.pSubpasses      = &subpass;
        info.dependencyCount = 1;
        info.pDependencies   = &dep;

        VkRenderPassResource res{};
        res.has_depth = desc.has_depth_stencil;
        res.color_formats.reserve(desc.color_attachments.size());
        for (const auto& a : desc.color_attachments)
        {
            res.color_formats.push_back(a.format);
        }
        if (vkCreateRenderPass(device_.logical(), &info, nullptr, &res.pass) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vkCreateRenderPass failed");
            return INVALID_HANDLE;
        }
        return render_passes_.add(res);
    }

    void GraphicsDeviceVulkan::destroy_render_pass(RenderPassHandle handle)
    {
        VkRenderPassResource res{};
        if (!render_passes_.remove(handle, &res))
        {
            return;
        }
        if (res.pass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device_.logical(), res.pass, nullptr);
        }
    }

    FramebufferHandle GraphicsDeviceVulkan::create_framebuffer(const FramebufferDesc& desc)
    {
        auto* rp = render_passes_.get(desc.render_pass);
        if (rp == nullptr)
        {
            ROVER_LOG_ERROR("create_framebuffer: invalid render_pass handle");
            return INVALID_HANDLE;
        }

        if (!desc.color_ranges.empty() && desc.color_ranges.size() != desc.color_attachments.size())
        {
            ROVER_LOG_ERROR("create_framebuffer: attachment range count mismatch");
            return INVALID_HANDLE;
        }
        std::vector<VkImageView>             views;
        std::vector<TextureSubresourceRange> color_ranges;
        TextureSubresourceRange              depth_range{};
        auto                                 attachment_view =
            [&](TextureHandle handle, TextureSubresourceRange requested, TextureSubresourceRange& range) {
                const auto* image = textures_.get(handle);
                if (!image ||
                    !normalize_texture_range(
                        image->engine_format, image->mip_levels, image->array_layers, requested, range) ||
                    range.mip_count != 1 || range.layer_count != 1 ||
                    desc.width != texture_mip_extent(image->extent.width, range.base_mip) ||
                    desc.height != texture_mip_extent(image->extent.height, range.base_mip))
                {
                    return VkImageView{VK_NULL_HANDLE};
                }
                return texture_view(handle, range, true);
            };
        for (usize i = 0; i < desc.color_attachments.size(); ++i)
        {
            TextureSubresourceRange range{};
            const auto              view =
                attachment_view(desc.color_attachments[i],
                                desc.color_ranges.empty() ? TEXTURE_BASE_SUBRESOURCE : desc.color_ranges[i],
                                range);
            if (!view || range.aspects != TextureAspect::Color)
            {
                ROVER_LOG_ERROR("create_framebuffer: invalid color attachment view");
                return INVALID_HANDLE;
            }
            views.push_back(view);
            color_ranges.push_back(range);
        }
        if (desc.depth_stencil != INVALID_HANDLE)
        {
            const auto view = attachment_view(desc.depth_stencil, desc.depth_range, depth_range);
            if (!view || depth_range.aspects != texture_aspects(textures_.get(desc.depth_stencil)->engine_format) ||
                depth_range.aspects == TextureAspect::Color)
            {
                ROVER_LOG_ERROR("create_framebuffer: invalid depth attachment view");
                return INVALID_HANDLE;
            }
            views.push_back(view);
        }

        VkFramebufferCreateInfo info{};
        info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass      = rp->pass;
        info.attachmentCount = static_cast<u32>(views.size());
        info.pAttachments    = views.empty() ? nullptr : views.data();
        info.width           = desc.width;
        info.height          = desc.height;
        info.layers          = 1;

        VkFramebufferResource res{};
        res.width             = desc.width;
        res.height            = desc.height;
        res.attachment_count  = static_cast<u32>(views.size());
        res.render_pass       = desc.render_pass;
        res.color_attachments = desc.color_attachments;
        res.color_ranges      = std::move(color_ranges);
        res.depth_range       = depth_range;
        res.depth_stencil     = desc.depth_stencil;
        if (vkCreateFramebuffer(device_.logical(), &info, nullptr, &res.framebuffer) != VK_SUCCESS)
        {
            ROVER_LOG_ERROR("vkCreateFramebuffer failed");
            return INVALID_HANDLE;
        }
        return framebuffers_.add(res);
    }

    void GraphicsDeviceVulkan::destroy_framebuffer(FramebufferHandle handle)
    {
        VkFramebufferResource res{};
        if (!framebuffers_.remove(handle, &res))
        {
            return;
        }
        if (res.framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device_.logical(), res.framebuffer, nullptr);
        }
    }

} // namespace rover
