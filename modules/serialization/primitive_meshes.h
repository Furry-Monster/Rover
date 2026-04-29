#pragma once

#include "modules/serialization/mesh_data.h"

namespace rover
{

    // ---------------------------------------------------------------------------
    // Phase 2 primitive mesh generators. These produce CPU-side `MeshData` that
    // can be uploaded by `MeshUploader` exactly like a parsed GLTF mesh, so the
    // downstream renderer code is decoupled from the source format. Real GLTF /
    // FBX importers slot in here once their vendor libraries are accepted.
    // ---------------------------------------------------------------------------

    // Unit cube centered at the origin with side length `size`.
    [[nodiscard]] MeshData make_cube(f32 size = 1.0f);

    // Unit quad in the XY plane (z = 0), normal pointing +Z.
    [[nodiscard]] MeshData make_quad(f32 size = 1.0f);

    // UV sphere with `latitude_segments` rings and `longitude_segments` slices.
    [[nodiscard]] MeshData make_sphere(f32 radius = 1.0f, u32 latitude_segments = 16, u32 longitude_segments = 24);

} // namespace rover
