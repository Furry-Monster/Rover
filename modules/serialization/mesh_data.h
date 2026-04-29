#pragma once

#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/typedefs.h"

#include <vector>

namespace rover
{

    // CPU-side mesh representation. Loaders (GLTF, .rmesh) and primitive
    // generators populate `MeshData`; `MeshUploader` then promotes it to GPU
    // buffers via the abstract `GraphicsDevice` API.
    //
    // Vertex layout for Phase 2:
    //   layout(location = 0) vec3 position
    //   layout(location = 1) vec3 normal
    //   layout(location = 2) vec2 uv
    //
    // Future phases (skinned meshes, vertex colors) extend the struct.
    struct MeshVertex
    {
        Vector3 position{};
        Vector3 normal{};
        Vector2 uv{};
    };

    struct MeshData
    {
        std::vector<MeshVertex> vertices;
        std::vector<u32>        indices;

        [[nodiscard]] u32 vertex_count() const noexcept { return static_cast<u32>(vertices.size()); }

        [[nodiscard]] u32 index_count() const noexcept { return static_cast<u32>(indices.size()); }

        [[nodiscard]] usize vertex_data_size() const noexcept { return vertices.size() * sizeof(MeshVertex); }

        [[nodiscard]] usize index_data_size() const noexcept { return indices.size() * sizeof(u32); }
    };

} // namespace rover
