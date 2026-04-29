#pragma once

#include "core/graphics/graphics_device.h"
#include "modules/serialization/texture_data.h"

namespace rover
{

    // Result of uploading a `TextureData` payload. Mirrors the `MeshComponent`
    // pattern: the caller (or future asset registry) is responsible for the
    // returned handles' lifetime and must call `destroy()` to free them.
    struct TextureGpuHandles
    {
        TextureHandle texture = INVALID_HANDLE;
        SamplerHandle sampler = INVALID_HANDLE;
    };

    class TextureUploader
    {
    public:
        // Build a `TextureGpuHandles` from CPU-side `TextureData`. Allocates a
        // GPU-only texture, uploads pixel data via `update_texture`, and creates
        // a default linear / repeat sampler.
        [[nodiscard]] static TextureGpuHandles upload(GraphicsDevice&    device,
                                                      const TextureData& data,
                                                      const char*        debug_name = nullptr);

        // Releases the GPU texture + sampler held by `handles`.
        static void destroy(GraphicsDevice& device, TextureGpuHandles& handles);
    };

} // namespace rover
