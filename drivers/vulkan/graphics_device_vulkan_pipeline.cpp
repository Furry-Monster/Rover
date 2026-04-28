#include "drivers/vulkan/graphics_device_vulkan.h"

#include "core/log/log.h"
#include "drivers/vulkan/vk_format.h"

#include <array>
#include <vector>

namespace rover {

PipelineHandle GraphicsDeviceVulkan::create_graphics_pipeline(const GraphicsPipelineDesc& desc) {
    auto* vs = shaders_.get(desc.vertex_shader);
    auto* fs = shaders_.get(desc.fragment_shader);
    auto* rp = render_passes_.get(desc.render_pass);
    if (vs == nullptr || fs == nullptr || rp == nullptr) {
        ROVER_LOG_ERROR("create_graphics_pipeline: invalid shader or render pass handle");
        return INVALID_HANDLE;
    }

    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs->module;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs->module;
    stages[1].pName  = "main";

    std::vector<VkVertexInputBindingDescription>   bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    bindings.reserve(desc.vertex_bindings.size());
    for (const auto& vb : desc.vertex_bindings) {
        VkVertexInputBindingDescription b{};
        b.binding   = vb.binding;
        b.stride    = vb.stride;
        b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings.push_back(b);
        for (const auto& attr : vb.attributes) {
            VkVertexInputAttributeDescription a{};
            a.binding  = vb.binding;
            a.location = attr.location;
            a.format   = to_vk_format(attr.format);
            a.offset   = attr.offset;
            attributes.push_back(a);
        }
    }

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount   = static_cast<u32>(bindings.size());
    vertex_input.pVertexBindingDescriptions      = bindings.empty() ? nullptr : bindings.data();
    vertex_input.vertexAttributeDescriptionCount = static_cast<u32>(attributes.size());
    vertex_input.pVertexAttributeDescriptions    = attributes.empty() ? nullptr : attributes.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology               = to_vk_topology(desc.topology);
    ia.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable        = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode             = VK_POLYGON_MODE_FILL;
    raster.cullMode                = to_vk_cull(desc.cull_mode);
    raster.frontFace               = to_vk_front_face(desc.front_face);
    raster.depthBiasEnable         = VK_FALSE;
    raster.lineWidth               = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ms.sampleShadingEnable  = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable       = desc.depth_test_enable && rp->has_depth ? VK_TRUE : VK_FALSE;
    depth.depthWriteEnable      = desc.depth_write_enable && rp->has_depth ? VK_TRUE : VK_FALSE;
    depth.depthCompareOp        = to_vk_compare(desc.depth_compare_op);
    depth.depthBoundsTestEnable = VK_FALSE;
    depth.stencilTestEnable     = VK_FALSE;

    const u32 color_attachment_count = static_cast<u32>(rp->color_formats.size());
    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments(color_attachment_count);
    for (u32 i = 0; i < color_attachment_count; ++i) {
        VkPipelineColorBlendAttachmentState& b = blend_attachments[i];
        b.blendEnable         = desc.blend_enable ? VK_TRUE : VK_FALSE;
        b.srcColorBlendFactor = to_vk_blend_factor(desc.src_color_blend);
        b.dstColorBlendFactor = to_vk_blend_factor(desc.dst_color_blend);
        b.colorBlendOp        = to_vk_blend_op(desc.color_blend_op);
        b.srcAlphaBlendFactor = to_vk_blend_factor(desc.src_alpha_blend);
        b.dstAlphaBlendFactor = to_vk_blend_factor(desc.dst_alpha_blend);
        b.alphaBlendOp        = to_vk_blend_op(desc.alpha_blend_op);
        b.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.logicOpEnable   = VK_FALSE;
    blend.attachmentCount = color_attachment_count;
    blend.pAttachments    = blend_attachments.empty() ? nullptr : blend_attachments.data();

    std::array<VkDynamicState, 2> dynamics = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = static_cast<u32>(dynamics.size());
    dynamic.pDynamicStates    = dynamics.data();

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkPipelineResource res{};
    if (vkCreatePipelineLayout(device_.logical(), &layout_info, nullptr, &res.layout) != VK_SUCCESS) {
        ROVER_LOG_ERROR("vkCreatePipelineLayout failed");
        return INVALID_HANDLE;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount          = static_cast<u32>(stages.size());
    pipeline_info.pStages             = stages.data();
    pipeline_info.pVertexInputState   = &vertex_input;
    pipeline_info.pInputAssemblyState = &ia;
    pipeline_info.pViewportState      = &viewport_state;
    pipeline_info.pRasterizationState = &raster;
    pipeline_info.pMultisampleState   = &ms;
    pipeline_info.pDepthStencilState  = rp->has_depth ? &depth : nullptr;
    pipeline_info.pColorBlendState    = &blend;
    pipeline_info.pDynamicState       = &dynamic;
    pipeline_info.layout              = res.layout;
    pipeline_info.renderPass          = rp->pass;
    pipeline_info.subpass             = 0;

    if (vkCreateGraphicsPipelines(device_.logical(), VK_NULL_HANDLE, 1,
                                  &pipeline_info, nullptr, &res.pipeline) != VK_SUCCESS) {
        ROVER_LOG_ERROR("vkCreateGraphicsPipelines failed");
        vkDestroyPipelineLayout(device_.logical(), res.layout, nullptr);
        return INVALID_HANDLE;
    }
    return pipelines_.add(res);
}

void GraphicsDeviceVulkan::destroy_pipeline(PipelineHandle handle) {
    VkPipelineResource res{};
    if (!pipelines_.remove(handle, &res)) return;
    if (res.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_.logical(), res.pipeline, nullptr);
    }
    if (res.layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_.logical(), res.layout, nullptr);
    }
}

} // namespace rover
