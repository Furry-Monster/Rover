#pragma once

#include "core/graphics/graphics_types.h"

namespace rover
{

    // Whole-resource access on the graphics queue. Undefined is an initial state,
    // never a barrier destination. Shader scopes conservatively cover all stages.
    enum class ResourceAccess : u8
    {
        Undefined,
        ShaderRead,
        StorageRead,
        StorageWrite,
        ColorAttachment,
        DepthStencilAttachment,
        TransferRead,
        TransferWrite,
        VertexRead,
        IndexRead,
        UniformRead,
        IndirectRead,
        HostRead,
        HostWrite,
        Present,
        Automatic, // PassBuilder::write default; resolved before device calls.
    };

    constexpr bool resource_access_is_write(ResourceAccess access)
    {
        return access == ResourceAccess::StorageWrite || access == ResourceAccess::ColorAttachment ||
               access == ResourceAccess::DepthStencilAttachment || access == ResourceAccess::TransferWrite ||
               access == ResourceAccess::HostWrite;
    }

    constexpr bool resource_access_is_read(ResourceAccess access)
    {
        return access == ResourceAccess::ShaderRead || access == ResourceAccess::StorageRead ||
               access == ResourceAccess::TransferRead || access == ResourceAccess::VertexRead ||
               access == ResourceAccess::IndexRead || access == ResourceAccess::UniformRead ||
               access == ResourceAccess::IndirectRead || access == ResourceAccess::HostRead;
    }

    constexpr bool resource_access_supports_buffer(ResourceAccess access)
    {
        return resource_access_is_read(access) || access == ResourceAccess::StorageWrite ||
               access == ResourceAccess::TransferWrite || access == ResourceAccess::HostWrite;
    }

    constexpr bool resource_access_supports_texture(ResourceAccess access)
    {
        return access == ResourceAccess::ShaderRead || access == ResourceAccess::StorageRead ||
               access == ResourceAccess::StorageWrite || access == ResourceAccess::ColorAttachment ||
               access == ResourceAccess::DepthStencilAttachment || access == ResourceAccess::TransferRead ||
               access == ResourceAccess::TransferWrite || access == ResourceAccess::Present;
    }

    struct BufferBarrier
    {
        BufferHandle   buffer = INVALID_HANDLE;
        ResourceAccess access = ResourceAccess::Undefined;
    };

    struct TextureBarrier
    {
        TextureHandle  texture = INVALID_HANDLE;
        ResourceAccess access  = ResourceAccess::Undefined;
    };

} // namespace rover
