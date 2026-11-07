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
    // A `RenderResource` is the *graph-side* description of a buffer or texture.
    // Real GPU handles are produced by the `GraphicsDevice` either ahead of time
    // (imported resources) or transiently during compile (managed resources). For
    // Phase 2 we only support imported resources -- the caller pre-creates the
    // GPU resource via `GraphicsDevice` and hands the handle to the frame graph.
    // Explicit versions borrow the imported handles; allocation is separate.
    // ---------------------------------------------------------------------------

    enum class ResourceKind : u8
    {
        Texture,
        Buffer,
    };

    enum class ResourceLifetime : u8
    {
        Imported,  // Owned by the caller, reused frame to frame (e.g. swapchain).
        Transient, // Reserved for graph-managed allocation.
    };

    using RenderResourceId                         = u32;
    constexpr RenderResourceId INVALID_RESOURCE_ID = 0xFFFFFFFFu;

    struct RenderResource
    {
        std::string      name;
        ResourceKind     kind     = ResourceKind::Texture;
        ResourceLifetime lifetime = ResourceLifetime::Imported;

        // Logical versions share their imported root's GPU handles. A version
        // orders an overwrite; it does not allocate a copy of the old contents.
        RenderResourceId physical_resource = INVALID_RESOURCE_ID;
        RenderResourceId previous_version  = INVALID_RESOURCE_ID;

        // Undefined leaves the last graph access unchanged; other values request
        // an export transition after all passes finish recording successfully.
        ResourceAccess final_access = ResourceAccess::Undefined;

        // Imported handles (one of texture / buffer is set depending on `kind`).
        TextureHandle texture = INVALID_HANDLE;
        BufferHandle  buffer  = INVALID_HANDLE;

        // Optional caller-created framebuffer containing this color texture.
        FramebufferHandle framebuffer = INVALID_HANDLE;

        // Geometry hints; used by transient allocator when implemented.
        u32    width  = 0;
        u32    height = 0;
        Format format = Format::UNDEFINED;
    };

    struct RenderResourceBarrier
    {
        RenderResourceId resource = INVALID_RESOURCE_ID;
        // Undefined here means the source is resolved from the driver's tracked
        // state, not that existing physical image contents should be discarded.
        ResourceAccess before = ResourceAccess::Undefined;
        ResourceAccess after  = ResourceAccess::Undefined;
    };

} // namespace rover
