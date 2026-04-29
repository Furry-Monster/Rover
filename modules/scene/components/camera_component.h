#pragma once

#include "core/math/mat4.h"
#include "core/typedefs.h"

namespace rover
{

    enum class CameraProjection : u8
    {
        Perspective,
        Orthographic,
    };

    // Per-entity camera parameters. Combined with the entity's
    // `TransformComponent` to produce the view matrix.
    struct CameraComponent
    {
        CameraProjection projection = CameraProjection::Perspective;

        // Perspective settings.
        f32 fov_y_radians = 1.0472f; // 60 degrees
        f32 near_plane    = 0.1f;
        f32 far_plane     = 1000.0f;

        // Orthographic settings.
        f32 ortho_size = 10.0f;

        // Set externally per-frame (driven by viewport / swapchain dimensions).
        f32 aspect_ratio = 16.0f / 9.0f;

        // Whether this camera is the primary one rendered to the screen this
        // frame. Phase 2 handles a single primary camera; multi-camera support
        // arrives with the editor scene-view in Phase 3.
        bool primary = true;

        [[nodiscard]] Mat4 projection_matrix() const noexcept
        {
            if (projection == CameraProjection::Orthographic)
            {
                const f32 h = ortho_size;
                const f32 w = h * aspect_ratio;
                return Mat4::ortho(-w, w, -h, h, near_plane, far_plane);
            }
            return Mat4::perspective(fov_y_radians, aspect_ratio, near_plane, far_plane);
        }
    };

} // namespace rover
