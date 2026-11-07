#pragma once

#include "core/graphics/graphics_desc.h"
#include "core/graphics/graphics_sync.h"
#include "core/graphics/graphics_types.h"
#include "core/typedefs.h"

#include <string>

namespace rover
{

    // ---------------------------------------------------------------------------
    // Frame Graph virtual resources.
    //
    // A RenderResource describes one logical version. Imported handles belong
    // to the caller; transient handles are resolved from a persistent pool after
    // compilation. Explicit versions share one physical root and GPU object.
    // ---------------------------------------------------------------------------

    enum class ResourceKind : u8
    {
        Texture,
        Buffer,
    };

    enum class ResourceLifetime : u8
    {
        Imported,  // Owned by the caller, reused frame to frame (e.g. swapchain).
        Transient, // Owned and leased by TransientResourcePool.
    };

    using RenderResourceId                         = u32;
    constexpr RenderResourceId INVALID_RESOURCE_ID = 0xFFFFFFFFu;

    struct RenderResourceExport
    {
        ResourceAccess          access = ResourceAccess::Undefined;
        TextureSubresourceRange range{};
    };

    struct RenderResource
    {
        std::string      name;
        ResourceKind     kind     = ResourceKind::Texture;
        ResourceLifetime lifetime = ResourceLifetime::Imported;

        // Logical versions share their physical root's GPU handles. A version
        // orders an overwrite; it does not allocate a copy of the old contents.
        RenderResourceId physical_resource = INVALID_RESOURCE_ID;
        RenderResourceId previous_version  = INVALID_RESOURCE_ID;

        // Undefined leaves the last graph access unchanged; other values request
        // an export transition after all passes finish recording successfully.
        ResourceAccess                    final_access = ResourceAccess::Undefined;
        std::vector<RenderResourceExport> exports;

        // Imported handles (one of texture / buffer is set depending on `kind`).
        TextureHandle texture = INVALID_HANDLE;
        BufferHandle  buffer  = INVALID_HANDLE;

        // Optional caller-created framebuffer containing this color texture.
        FramebufferHandle framebuffer = INVALID_HANDLE;

        // Base dimensions and format used by attachment validation and allocation.
        u32    width  = 0;
        u32    height = 0;
        Format format = Format::UNDEFINED;

        TextureDesc texture_desc{};
        BufferDesc  buffer_desc{};
        u32         first_use = 0xFFFFFFFFu;
        u32         last_use  = 0xFFFFFFFFu;
    };

    using RenderBindGroupId                           = u32;
    constexpr RenderBindGroupId INVALID_BIND_GROUP_ID = 0xFFFFFFFFu;

    struct RenderBinding
    {
        BindGroupEntry   entry{};
        RenderResourceId resource = INVALID_RESOURCE_ID;
    };

    struct RenderBindGroup
    {
        BindGroupLayoutHandle      layout = INVALID_HANDLE;
        std::vector<RenderBinding> entries;
        BindGroupHandle            handle = INVALID_HANDLE;
    };

    struct RenderResourceBarrier
    {
        RenderResourceId resource = INVALID_RESOURCE_ID;
        // Undefined here means the source is resolved from the driver's tracked
        // state, not that existing physical image contents should be discarded.
        ResourceAccess          before = ResourceAccess::Undefined;
        ResourceAccess          after  = ResourceAccess::Undefined;
        TextureSubresourceRange range{};
    };

} // namespace rover
