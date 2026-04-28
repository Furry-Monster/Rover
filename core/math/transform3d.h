#pragma once

#include "core/typedefs.h"
#include "core/math/vector3.h"
#include "core/math/quat.h"
#include "core/math/mat4.h"

namespace rover {

struct Transform3D {
    Vector3 origin{};
    Quat    rotation{};
    Vector3 scale{1.0f, 1.0f, 1.0f};

    constexpr Transform3D() noexcept = default;
    constexpr Transform3D(const Vector3& origin, const Quat& rotation,
                          const Vector3& scale) noexcept
        : origin(origin), rotation(rotation), scale(scale) {}

    [[nodiscard]] Mat4 to_mat4() const noexcept {
        Mat4 t = Mat4::translate(origin);
        Mat4 r = rotation.to_mat4();
        Mat4 s = Mat4::scale(scale);
        return t * r * s;
    }

    [[nodiscard]] Transform3D inverse() const noexcept {
        Quat inv_rot = rotation.inverse();
        Vector3 inv_scale{1.0f / scale.x(), 1.0f / scale.y(), 1.0f / scale.z()};
        Vector3 inv_origin = inv_rot * ((-origin) * inv_scale);
        return {inv_origin, inv_rot, inv_scale};
    }

    [[nodiscard]] Vector3 transform_point(const Vector3& point) const noexcept {
        return origin + rotation * (point * scale);
    }

    [[nodiscard]] Vector3 transform_direction(const Vector3& dir) const noexcept {
        return rotation * dir;
    }

    [[nodiscard]] static constexpr Transform3D identity() noexcept { return {}; }
};

} // namespace rover
