#pragma once

#include "core/graphics/graphics_types.h"
#include "core/math/vector4.h"

namespace rover
{

    // Basic Phase 2 material descriptor. Holds a pipeline + bind group layout +
    // optional albedo texture/sampler. Replaces the hand-rolled PSO/Pipeline
    // state hard-coded in `main/main.cpp`'s triangle demo.
    //
    // `Material` is intentionally a pure value type: the ownership model for
    // Pipeline / BindGroup handles lives one level up (Sprint 2.7's demo and
    // future Asset Registry).
    struct Material
    {
        PipelineHandle        pipeline          = INVALID_HANDLE;
        PipelineLayoutHandle  pipeline_layout   = INVALID_HANDLE;
        BindGroupLayoutHandle bind_group_layout = INVALID_HANDLE;
        BindGroupHandle       bind_group        = INVALID_HANDLE;

        // Constant uniform values; copied into the bind group's UBO each frame.
        Vector4 albedo_color{1.0f, 1.0f, 1.0f, 1.0f};
        f32     metallic  = 0.0f;
        f32     roughness = 0.5f;

        // Optional sampled albedo texture. INVALID_HANDLE = use albedo_color only.
        TextureHandle albedo_texture = INVALID_HANDLE;
        SamplerHandle albedo_sampler = INVALID_HANDLE;
    };

} // namespace rover
