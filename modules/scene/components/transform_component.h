#pragma once

#include "core/math/quat.h"
#include "core/math/transform3d.h"
#include "core/math/vector3.h"

namespace rover
{

    // 3D transform component. Stored as origin / rotation / scale rather than
    // a 4x4 matrix so child transforms can be composed without matrix
    // decomposition. Use `to_mat4()` when uploading to the GPU.
    struct TransformComponent
    {
        Vector3 position{0.0f, 0.0f, 0.0f};
        Quat    rotation{};
        Vector3 scale{1.0f, 1.0f, 1.0f};

        [[nodiscard]] Transform3D as_transform3d() const noexcept { return {position, rotation, scale}; }

        [[nodiscard]] Mat4 to_mat4() const noexcept { return as_transform3d().to_mat4(); }
    };

} // namespace rover
