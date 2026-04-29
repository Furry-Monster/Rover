#pragma once

#include "core/graphics/graphics_types.h"

#include <volk.h>

namespace rover
{

    [[nodiscard]] VkFormat              to_vk_format(Format format);
    [[nodiscard]] Format                from_vk_format(VkFormat format);
    [[nodiscard]] VkPrimitiveTopology   to_vk_topology(PrimitiveTopology topo);
    [[nodiscard]] VkCullModeFlags       to_vk_cull(CullMode cull);
    [[nodiscard]] VkFrontFace           to_vk_front_face(FrontFace ff);
    [[nodiscard]] VkCompareOp           to_vk_compare(CompareOp op);
    [[nodiscard]] VkBlendFactor         to_vk_blend_factor(BlendFactor f);
    [[nodiscard]] VkBlendOp             to_vk_blend_op(BlendOp op);
    [[nodiscard]] VkAttachmentLoadOp    to_vk_load_op(LoadOp op);
    [[nodiscard]] VkAttachmentStoreOp   to_vk_store_op(StoreOp op);
    [[nodiscard]] VkFilter              to_vk_filter(Filter f);
    [[nodiscard]] VkSamplerAddressMode  to_vk_address_mode(SamplerAddressMode m);
    [[nodiscard]] VkIndexType           to_vk_index_type(IndexType t);
    [[nodiscard]] VkBufferUsageFlags    to_vk_buffer_usage(BufferUsage u);
    [[nodiscard]] VkImageUsageFlags     to_vk_image_usage(TextureUsage u);
    [[nodiscard]] VkShaderStageFlagBits to_vk_shader_stage(ShaderStage s);
    [[nodiscard]] VkShaderStageFlags    to_vk_shader_stage_flags(ShaderStage s);
    [[nodiscard]] VkImageType           to_vk_image_type(TextureType t);
    [[nodiscard]] VkImageViewType       to_vk_image_view_type(TextureType t);

} // namespace rover
