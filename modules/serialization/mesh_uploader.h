#pragma once

#include "core/graphics/graphics_device.h"
#include "modules/scene/components/mesh_component.h"
#include "modules/serialization/mesh_data.h"

namespace rover
{

    // MeshUploader: promotes CPU-side `MeshData` to GPU buffers via the abstract
    // `GraphicsDevice` API. Returns a populated `MeshComponent` whose handles are
    // owned by the device until destroy_buffer is called.
    //
    // The component does NOT take ownership semantics here; callers are expected
    // to track the MeshComponent and free its buffers on shutdown. The future
    // AssetRegistry will manage lifetime by reference counting.
    class MeshUploader
    {
    public:
        // Build a `MeshComponent` from CPU-side `MeshData`. Uploads vertex and
        // index data to GPU-only buffers via `update_buffer`. On any allocation
        // failure returns a default-constructed (invalid-handle) MeshComponent.
        [[nodiscard]] static MeshComponent upload(GraphicsDevice& device,
                                                  const MeshData& data,
                                                  const char*     debug_name = nullptr);

        // Releases the GPU buffers held by `mc` (vertex + index). Texture/sampler
        // handles are left alone -- the caller / asset registry tracks those.
        static void destroy_buffers(GraphicsDevice& device, MeshComponent& mc);
    };

} // namespace rover
