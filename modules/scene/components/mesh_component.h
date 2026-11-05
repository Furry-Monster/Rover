#pragma once

#include "core/graphics/graphics_types.h"

#include <string>

namespace rover
{

    // GPU handles are borrowed runtime data. Only the stable mesh_asset path is
    // serialized; the resource owner must outlive every draw using these handles.
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

        std::string mesh_asset; // e.g. builtin://meshes/cube
    };

} // namespace rover
