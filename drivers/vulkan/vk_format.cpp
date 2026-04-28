#include "drivers/vulkan/vk_format.h"

#include "core/typedefs.h"

namespace rover {

VkFormat to_vk_format(Format format) {
    switch (format) {
        case Format::UNDEFINED:           return VK_FORMAT_UNDEFINED;
        case Format::R8_UNORM:            return VK_FORMAT_R8_UNORM;
        case Format::R8G8B8A8_UNORM:      return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::R8G8B8A8_SRGB:       return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::B8G8R8A8_UNORM:      return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::B8G8R8A8_SRGB:       return VK_FORMAT_B8G8R8A8_SRGB;
        case Format::R16G16B16A16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Format::R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::R32G32B32_SFLOAT:    return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::R32G32_SFLOAT:       return VK_FORMAT_R32G32_SFLOAT;
        case Format::D32_SFLOAT:          return VK_FORMAT_D32_SFLOAT;
        case Format::D24_UNORM_S8_UINT:   return VK_FORMAT_D24_UNORM_S8_UINT;
        case Format::D32_SFLOAT_S8_UINT:  return VK_FORMAT_D32_SFLOAT_S8_UINT;
    }
    return VK_FORMAT_UNDEFINED;
}

Format from_vk_format(VkFormat format) {
    switch (format) {
        case VK_FORMAT_UNDEFINED:           return Format::UNDEFINED;
        case VK_FORMAT_R8_UNORM:            return Format::R8_UNORM;
        case VK_FORMAT_R8G8B8A8_UNORM:      return Format::R8G8B8A8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:       return Format::R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:      return Format::B8G8R8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:       return Format::B8G8R8A8_SRGB;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return Format::R16G16B16A16_SFLOAT;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return Format::R32G32B32A32_SFLOAT;
        case VK_FORMAT_R32G32B32_SFLOAT:    return Format::R32G32B32_SFLOAT;
        case VK_FORMAT_R32G32_SFLOAT:       return Format::R32G32_SFLOAT;
        case VK_FORMAT_D32_SFLOAT:          return Format::D32_SFLOAT;
        case VK_FORMAT_D24_UNORM_S8_UINT:   return Format::D24_UNORM_S8_UINT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:  return Format::D32_SFLOAT_S8_UINT;
        default:                            return Format::UNDEFINED;
    }
}

VkPrimitiveTopology to_vk_topology(PrimitiveTopology topo) {
    switch (topo) {
        case PrimitiveTopology::TriangleList:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::LineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::PointList:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkCullModeFlags to_vk_cull(CullMode cull) {
    switch (cull) {
        case CullMode::None:         return VK_CULL_MODE_NONE;
        case CullMode::Front:        return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:         return VK_CULL_MODE_BACK_BIT;
        case CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
    }
    return VK_CULL_MODE_NONE;
}

VkFrontFace to_vk_front_face(FrontFace ff) {
    switch (ff) {
        case FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        case FrontFace::Clockwise:        return VK_FRONT_FACE_CLOCKWISE;
    }
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

VkCompareOp to_vk_compare(CompareOp op) {
    switch (op) {
        case CompareOp::Never:          return VK_COMPARE_OP_NEVER;
        case CompareOp::Less:           return VK_COMPARE_OP_LESS;
        case CompareOp::Equal:          return VK_COMPARE_OP_EQUAL;
        case CompareOp::LessOrEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater:        return VK_COMPARE_OP_GREATER;
        case CompareOp::NotEqual:       return VK_COMPARE_OP_NOT_EQUAL;
        case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Always:         return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_LESS;
}

VkBlendFactor to_vk_blend_factor(BlendFactor f) {
    switch (f) {
        case BlendFactor::Zero:                  return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One:                   return VK_BLEND_FACTOR_ONE;
        case BlendFactor::SrcColor:              return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:              return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::OneMinusDstColor:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:              return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:              return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::ConstantColor:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case BlendFactor::OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case BlendFactor::SrcAlphaSaturate:      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    }
    return VK_BLEND_FACTOR_ZERO;
}

VkBlendOp to_vk_blend_op(BlendOp op) {
    switch (op) {
        case BlendOp::Add:             return VK_BLEND_OP_ADD;
        case BlendOp::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::Min:             return VK_BLEND_OP_MIN;
        case BlendOp::Max:             return VK_BLEND_OP_MAX;
    }
    return VK_BLEND_OP_ADD;
}

VkAttachmentLoadOp to_vk_load_op(LoadOp op) {
    switch (op) {
        case LoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp to_vk_store_op(StoreOp op) {
    switch (op) {
        case StoreOp::Store:    return VK_ATTACHMENT_STORE_OP_STORE;
        case StoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkFilter to_vk_filter(Filter f) {
    switch (f) {
        case Filter::Nearest: return VK_FILTER_NEAREST;
        case Filter::Linear:  return VK_FILTER_LINEAR;
    }
    return VK_FILTER_NEAREST;
}

VkSamplerAddressMode to_vk_address_mode(SamplerAddressMode m) {
    switch (m) {
        case SamplerAddressMode::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerAddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SamplerAddressMode::ClampToEdge:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerAddressMode::ClampToBorder:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkIndexType to_vk_index_type(IndexType t) {
    switch (t) {
        case IndexType::Uint16: return VK_INDEX_TYPE_UINT16;
        case IndexType::Uint32: return VK_INDEX_TYPE_UINT32;
    }
    return VK_INDEX_TYPE_UINT32;
}

VkBufferUsageFlags to_vk_buffer_usage(BufferUsage u) {
    using U = std::underlying_type_t<BufferUsage>;
    const U bits = static_cast<U>(u);
    VkBufferUsageFlags out = 0;
    if (bits & static_cast<U>(BufferUsage::Vertex))   out |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (bits & static_cast<U>(BufferUsage::Index))    out |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (bits & static_cast<U>(BufferUsage::Uniform))  out |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (bits & static_cast<U>(BufferUsage::Storage))  out |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (bits & static_cast<U>(BufferUsage::Indirect)) out |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (bits & static_cast<U>(BufferUsage::Transfer)) {
        out |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    return out;
}

VkImageUsageFlags to_vk_image_usage(TextureUsage u) {
    using U = std::underlying_type_t<TextureUsage>;
    const U bits = static_cast<U>(u);
    VkImageUsageFlags out = 0;
    if (bits & static_cast<U>(TextureUsage::Sampled))                out |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (bits & static_cast<U>(TextureUsage::Storage))                out |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (bits & static_cast<U>(TextureUsage::ColorAttachment))        out |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (bits & static_cast<U>(TextureUsage::DepthStencilAttachment)) out |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (bits & static_cast<U>(TextureUsage::TransferSrc))            out |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (bits & static_cast<U>(TextureUsage::TransferDst))            out |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return out;
}

VkShaderStageFlagBits to_vk_shader_stage(ShaderStage s) {
    switch (s) {
        case ShaderStage::Vertex:      return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment:    return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Compute:     return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::Geometry:    return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::TessControl: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::TessEval:    return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }
    return VK_SHADER_STAGE_VERTEX_BIT;
}

VkShaderStageFlags to_vk_shader_stage_flags(ShaderStage s) {
    using U = std::underlying_type_t<ShaderStage>;
    const U bits = static_cast<U>(s);
    VkShaderStageFlags out = 0;
    if (bits & static_cast<U>(ShaderStage::Vertex))      out |= VK_SHADER_STAGE_VERTEX_BIT;
    if (bits & static_cast<U>(ShaderStage::Fragment))    out |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (bits & static_cast<U>(ShaderStage::Compute))     out |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (bits & static_cast<U>(ShaderStage::Geometry))    out |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (bits & static_cast<U>(ShaderStage::TessControl)) out |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (bits & static_cast<U>(ShaderStage::TessEval))    out |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    return out;
}

VkImageType to_vk_image_type(TextureType t) {
    switch (t) {
        case TextureType::Texture2D:      return VK_IMAGE_TYPE_2D;
        case TextureType::Texture3D:      return VK_IMAGE_TYPE_3D;
        case TextureType::TextureCube:    return VK_IMAGE_TYPE_2D;
        case TextureType::Texture2DArray: return VK_IMAGE_TYPE_2D;
    }
    return VK_IMAGE_TYPE_2D;
}

VkImageViewType to_vk_image_view_type(TextureType t) {
    switch (t) {
        case TextureType::Texture2D:      return VK_IMAGE_VIEW_TYPE_2D;
        case TextureType::Texture3D:      return VK_IMAGE_VIEW_TYPE_3D;
        case TextureType::TextureCube:    return VK_IMAGE_VIEW_TYPE_CUBE;
        case TextureType::Texture2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }
    return VK_IMAGE_VIEW_TYPE_2D;
}

} // namespace rover
