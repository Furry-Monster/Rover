#pragma once

#include "core/math/vector3.h"
#include "core/typedefs.h"

namespace rover
{

    enum class LightType : u8
    {
        Directional,
        Point,
        Spot,
    };

    // Light source. Phase 2 supports a single directional light in the forward
    // pass; spot/point are stored for completeness but only consumed when the
    // renderer learns the multi-light path (Phase 3).
    struct LightComponent
    {
        LightType type = LightType::Directional;

        Vector3 color{1.0f, 1.0f, 1.0f};
        f32     intensity = 1.0f;

        // Used by directional/spot lights; recomputed from the entity's
        // TransformComponent rotation each frame.
        Vector3 direction{0.0f, -1.0f, 0.0f};

        // Point/spot-only attenuation parameters.
        f32 range               = 10.0f;
        f32 spot_cone_inner_cos = 0.95f;
        f32 spot_cone_outer_cos = 0.85f;
    };

} // namespace rover
