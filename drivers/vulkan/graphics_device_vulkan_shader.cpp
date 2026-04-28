#include "drivers/vulkan/graphics_device_vulkan.h"

#include "core/log/log.h"
#include "drivers/vulkan/vk_format.h"

#include <vector>

namespace rover {

ShaderHandle GraphicsDeviceVulkan::create_shader(const ShaderDesc& desc) {
    if (desc.bytecode == nullptr || desc.bytecode_size == 0) {
        ROVER_LOG_ERROR("create_shader: empty bytecode");
        return INVALID_HANDLE;
    }
    if ((desc.bytecode_size % 4) != 0) {
        ROVER_LOG_ERROR("create_shader: bytecode_size must be a multiple of 4 (SPIR-V words)");
        return INVALID_HANDLE;
    }

    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = desc.bytecode_size;
    info.pCode    = reinterpret_cast<const u32*>(desc.bytecode);

    VkShaderResource res{};
    res.stage = to_vk_shader_stage(desc.stage);
    if (vkCreateShaderModule(device_.logical(), &info, nullptr, &res.module) != VK_SUCCESS) {
        ROVER_LOG_ERROR("vkCreateShaderModule failed");
        return INVALID_HANDLE;
    }
    return shaders_.add(res);
}

void GraphicsDeviceVulkan::destroy_shader(ShaderHandle handle) {
    VkShaderResource res{};
    if (!shaders_.remove(handle, &res)) return;
    if (res.module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_.logical(), res.module, nullptr);
    }
}

RenderPassHandle GraphicsDeviceVulkan::create_render_pass(const RenderPassDesc& desc) {
    std::vector<VkAttachmentDescription> attachments;
    attachments.reserve(desc.color_attachments.size() + (desc.has_depth_stencil ? 1u : 0u));

    std::vector<VkAttachmentReference> color_refs;
    color_refs.reserve(desc.color_attachments.size());

    for (usize i = 0; i < desc.color_attachments.size(); ++i) {
        const auto& a = desc.color_attachments[i];
        VkAttachmentDescription att{};
        att.format         = to_vk_format(a.format);
        att.samples        = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp         = to_vk_load_op(a.load_op);
        att.storeOp        = to_vk_store_op(a.store_op);
        att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.initialLayout  = a.load_op == LoadOp::Load
                             ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                             : VK_IMAGE_LAYOUT_UNDEFINED;
        att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments.push_back(att);

        VkAttachmentReference ref{};
        ref.attachment = static_cast<u32>(i);
        ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_refs.push_back(ref);
    }

    VkAttachmentReference depth_ref{};
    if (desc.has_depth_stencil) {
        VkAttachmentDescription att{};
        att.format         = to_vk_format(desc.depth_stencil.format);
        att.samples        = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp         = to_vk_load_op(desc.depth_stencil.load_op);
        att.storeOp        = to_vk_store_op(desc.depth_stencil.store_op);
        att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        att.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(att);

        depth_ref.attachment = static_cast<u32>(attachments.size() - 1);
        depth_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<u32>(color_refs.size());
    subpass.pColorAttachments    = color_refs.empty() ? nullptr : color_refs.data();
    subpass.pDepthStencilAttachment = desc.has_depth_stencil ? &depth_ref : nullptr;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

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
    for (const auto& a : desc.color_attachments) {
        res.color_formats.push_back(a.format);
    }
    if (vkCreateRenderPass(device_.logical(), &info, nullptr, &res.pass) != VK_SUCCESS) {
        ROVER_LOG_ERROR("vkCreateRenderPass failed");
        return INVALID_HANDLE;
    }
    return render_passes_.add(res);
}

void GraphicsDeviceVulkan::destroy_render_pass(RenderPassHandle handle) {
    VkRenderPassResource res{};
    if (!render_passes_.remove(handle, &res)) return;
    if (res.pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_.logical(), res.pass, nullptr);
    }
}

FramebufferHandle GraphicsDeviceVulkan::create_framebuffer(const FramebufferDesc& desc) {
    auto* rp = render_passes_.get(desc.render_pass);
    if (rp == nullptr) {
        ROVER_LOG_ERROR("create_framebuffer: invalid render_pass handle");
        return INVALID_HANDLE;
    }

    std::vector<VkImageView> views;
    views.reserve(desc.color_attachments.size() + 1);
    for (TextureHandle h : desc.color_attachments) {
        auto* tex = textures_.get(h);
        if (tex == nullptr || tex->view == VK_NULL_HANDLE) {
            ROVER_LOG_ERROR("create_framebuffer: invalid color attachment");
            return INVALID_HANDLE;
        }
        views.push_back(tex->view);
    }
    if (desc.depth_stencil != INVALID_HANDLE) {
        auto* tex = textures_.get(desc.depth_stencil);
        if (tex == nullptr || tex->view == VK_NULL_HANDLE) {
            ROVER_LOG_ERROR("create_framebuffer: invalid depth attachment");
            return INVALID_HANDLE;
        }
        views.push_back(tex->view);
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
    res.width            = desc.width;
    res.height           = desc.height;
    res.attachment_count = static_cast<u32>(views.size());
    res.render_pass      = desc.render_pass;
    if (vkCreateFramebuffer(device_.logical(), &info, nullptr, &res.framebuffer) != VK_SUCCESS) {
        ROVER_LOG_ERROR("vkCreateFramebuffer failed");
        return INVALID_HANDLE;
    }
    return framebuffers_.add(res);
}

void GraphicsDeviceVulkan::destroy_framebuffer(FramebufferHandle handle) {
    VkFramebufferResource res{};
    if (!framebuffers_.remove(handle, &res)) return;
    if (res.framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_.logical(), res.framebuffer, nullptr);
    }
}

} // namespace rover
