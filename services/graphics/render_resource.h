#pragma once

#include "core/graphics/graphics_desc.h"
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
    // Transient allocation will be added in Phase 3.
    // ---------------------------------------------------------------------------

    enum class ResourceKind : u8
    {
        Texture,
        Buffer,
    };

    enum class ResourceLifetime : u8
    {
        Imported,  // Owned by the caller, reused frame to frame (e.g. swapchain).
        Transient, // Allocated by the frame graph for one frame (Phase 3+).
    };

    struct RenderResource
    {
        std::string      name;
        ResourceKind     kind     = ResourceKind::Texture;
        ResourceLifetime lifetime = ResourceLifetime::Imported;

        // Imported handles (one of texture / buffer is set depending on `kind`).
        TextureHandle texture = INVALID_HANDLE;
        BufferHandle  buffer  = INVALID_HANDLE;

        // Optional pre-created framebuffer for color/depth attachments. The
        // simple Phase 2 path uses `GraphicsService::create_framebuffer_for()` to
        // build this; future graph compile will manage automatically.
        FramebufferHandle framebuffer = INVALID_HANDLE;

        // Geometry hints; used by transient allocator when implemented.
        u32    width  = 0;
        u32    height = 0;
        Format format = Format::UNDEFINED;
    };

    using RenderResourceId                         = u32;
    constexpr RenderResourceId INVALID_RESOURCE_ID = 0xFFFFFFFFu;

} // namespace rover
