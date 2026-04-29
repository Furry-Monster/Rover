#pragma once

#include "core/graphics/graphics_types.h"

namespace rover
{

    // Reference to a GPU mesh + material. Phase 2 has no Asset Registry yet so
    // the handles are raw GraphicsDevice handles; Phase 3 will replace them with
    // AssetID lookups resolved per-frame.
    struct MeshComponent
    {
        BufferHandle vertex_buffer = INVALID_HANDLE;
        BufferHandle index_buffer  = INVALID_HANDLE;
        u32          vertex_count  = 0;
        u32          index_count   = 0;
        IndexType    index_type    = IndexType::Uint32;

        // Optional sampled texture + sampler for the basic Phase 2 forward pass.
        TextureHandle albedo_texture = INVALID_HANDLE;
        SamplerHandle albedo_sampler = INVALID_HANDLE;
    };

} // namespace rover
