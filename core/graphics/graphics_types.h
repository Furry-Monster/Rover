#pragma once

#include "core/typedefs.h"

#include <type_traits>

namespace rover
{

    // ---------------------------------------------------------------------------
    // Opaque GPU resource handles (0 = invalid)
    // ---------------------------------------------------------------------------
    using BufferHandle          = u64;
    using TextureHandle         = u64;
    using SamplerHandle         = u64;
    using ShaderHandle          = u64;
    using PipelineHandle        = u64;
    using PipelineLayoutHandle  = u64;
    using BindGroupLayoutHandle = u64;
    using BindGroupHandle       = u64;
    using RenderPassHandle      = u64;
    using FramebufferHandle     = u64;
    using CommandListHandle     = u64;

    static constexpr u64 INVALID_HANDLE = 0;

// ---------------------------------------------------------------------------
// Bitwise operators for flag enums
// ---------------------------------------------------------------------------
#define ROVER_DEFINE_FLAG_OPERATORS(EnumType)                                                                          \
    inline constexpr EnumType operator|(EnumType a, EnumType b)                                                        \
    {                                                                                                                  \
        using U = std::underlying_type_t<EnumType>;                                                                    \
        return static_cast<EnumType>(static_cast<U>(a) | static_cast<U>(b));                                           \
    }                                                                                                                  \
    inline constexpr EnumType operator&(EnumType a, EnumType b)                                                        \
    {                                                                                                                  \
        using U = std::underlying_type_t<EnumType>;                                                                    \
        return static_cast<EnumType>(static_cast<U>(a) & static_cast<U>(b));                                           \
    }                                                                                                                  \
    inline constexpr EnumType operator^(EnumType a, EnumType b)                                                        \
    {                                                                                                                  \
        using U = std::underlying_type_t<EnumType>;                                                                    \
        return static_cast<EnumType>(static_cast<U>(a) ^ static_cast<U>(b));                                           \
    }                                                                                                                  \
    inline constexpr EnumType operator~(EnumType a)                                                                    \
    {                                                                                                                  \
        using U = std::underlying_type_t<EnumType>;                                                                    \
        return static_cast<EnumType>(~static_cast<U>(a));                                                              \
    }                                                                                                                  \
    inline constexpr EnumType& operator|=(EnumType& a, EnumType b)                                                     \
    {                                                                                                                  \
        return a = a | b;                                                                                              \
    }                                                                                                                  \
    inline constexpr EnumType& operator&=(EnumType& a, EnumType b)                                                     \
    {                                                                                                                  \
        return a = a & b;                                                                                              \
    }                                                                                                                  \
    inline constexpr EnumType& operator^=(EnumType& a, EnumType b)                                                     \
    {                                                                                                                  \
        return a = a ^ b;                                                                                              \
    }

    // ---------------------------------------------------------------------------
    // Pixel / vertex formats
    // ---------------------------------------------------------------------------
    enum class Format : u32
    {
        UNDEFINED = 0,
        R8_UNORM,
        R8G8B8A8_UNORM,
        R8G8B8A8_SRGB,
        B8G8R8A8_UNORM,
        B8G8R8A8_SRGB,
        R16G16B16A16_SFLOAT,
        R32G32B32A32_SFLOAT,
        R32G32B32_SFLOAT,
        R32G32_SFLOAT,
        D32_SFLOAT,
        D24_UNORM_S8_UINT,
        D32_SFLOAT_S8_UINT,
    };

    // ---------------------------------------------------------------------------
    // Buffer usage flags
    // ---------------------------------------------------------------------------
    enum class BufferUsage : u32
    {
        Vertex   = 1 << 0,
        Index    = 1 << 1,
        Uniform  = 1 << 2,
        Storage  = 1 << 3,
        Indirect = 1 << 4,
        Transfer = 1 << 5,
    };
    ROVER_DEFINE_FLAG_OPERATORS(BufferUsage)

    // ---------------------------------------------------------------------------
    // Texture usage flags
    // ---------------------------------------------------------------------------
    enum class TextureUsage : u32
    {
        Sampled                = 1 << 0,
        Storage                = 1 << 1,
        ColorAttachment        = 1 << 2,
        DepthStencilAttachment = 1 << 3,
        TransferSrc            = 1 << 4,
        TransferDst            = 1 << 5,
    };
    ROVER_DEFINE_FLAG_OPERATORS(TextureUsage)

    // ---------------------------------------------------------------------------
    // Shader stage flags
    // ---------------------------------------------------------------------------
    enum class ShaderStage : u32
    {
        Vertex      = 1 << 0,
        Fragment    = 1 << 1,
        Compute     = 1 << 2,
        Geometry    = 1 << 3,
        TessControl = 1 << 4,
        TessEval    = 1 << 5,
    };
    ROVER_DEFINE_FLAG_OPERATORS(ShaderStage)

    // ---------------------------------------------------------------------------
    // Fixed enums
    // ---------------------------------------------------------------------------
    enum class PrimitiveTopology : u8
    {
        TriangleList,
        TriangleStrip,
        LineList,
        LineStrip,
        PointList,
    };

    enum class CullMode : u8
    {
        None,
        Front,
        Back,
        FrontAndBack,
    };

    enum class FrontFace : u8
    {
        CounterClockwise,
        Clockwise,
    };

    enum class CompareOp : u8
    {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always,
    };

    enum class BlendFactor : u8
    {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
        SrcAlphaSaturate,
    };

    enum class BlendOp : u8
    {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max,
    };

    enum class LoadOp : u8
    {
        Load,
        Clear,
        DontCare,
    };

    enum class StoreOp : u8
    {
        Store,
        DontCare,
    };

    enum class TextureType : u8
    {
        Texture2D,
        Texture3D,
        TextureCube,
        Texture2DArray,
    };

    enum class Filter : u8
    {
        Nearest,
        Linear,
    };

    enum class SamplerAddressMode : u8
    {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder,
    };

    enum class IndexType : u8
    {
        Uint16,
        Uint32,
    };

    enum class MemoryUsage : u8
    {
        GpuOnly,
        CpuToGpu,
        GpuToCpu,
    };

    // ---------------------------------------------------------------------------
    // Bind group: type of resource declared at a given binding slot.
    //
    // Maps to Vulkan's VkDescriptorType but stays API-agnostic so that D3D12
    // (root-signature) and WebGPU (BindGroupLayout) implementations only need
    // to translate the small enum below.
    // ---------------------------------------------------------------------------
    enum class BindingType : u8
    {
        UniformBuffer,
        StorageBuffer,
        SampledTexture,
        StorageTexture,
        Sampler,
    };

} // namespace rover
