#pragma once

#include "core/math/mat4.h"
#include "core/math/vector3.h"
#include "core/typedefs.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace rover
{

    struct Quat
    {
        glm::quat q{1.0f, 0.0f, 0.0f, 0.0f};

        constexpr Quat() noexcept = default;

        constexpr Quat(f32 w, f32 x, f32 y, f32 z) noexcept : q(w, x, y, z) {}

        constexpr Quat(glm::quat quat) noexcept : q(quat) {}

        [[nodiscard]] constexpr f32& w() noexcept { return q.w; }

        [[nodiscard]] constexpr f32& x() noexcept { return q.x; }

        [[nodiscard]] constexpr f32& y() noexcept { return q.y; }

        [[nodiscard]] constexpr f32& z() noexcept { return q.z; }

        [[nodiscard]] constexpr f32 w() const noexcept { return q.w; }

        [[nodiscard]] constexpr f32 x() const noexcept { return q.x; }

        [[nodiscard]] constexpr f32 y() const noexcept { return q.y; }

        [[nodiscard]] constexpr f32 z() const noexcept { return q.z; }

        [[nodiscard]] constexpr operator glm::quat() const noexcept { return q; }

        // Arithmetic
        [[nodiscard]] constexpr Quat operator*(const Quat& rhs) const noexcept { return {q * rhs.q}; }

        Quat& operator*=(const Quat& rhs) noexcept
        {
            q *= rhs.q;
            return *this;
        }

        [[nodiscard]] Vector3 operator*(const Vector3& vec) const noexcept { return {glm::rotate(q, vec.v)}; }

        [[nodiscard]] constexpr bool operator==(const Quat& rhs) const noexcept { return q == rhs.q; }

        [[nodiscard]] constexpr bool operator!=(const Quat& rhs) const noexcept { return q != rhs.q; }

        // Operations
        [[nodiscard]] Quat normalized() const noexcept { return {glm::normalize(q)}; }

        [[nodiscard]] Quat inverse() const noexcept { return {glm::inverse(q)}; }

        [[nodiscard]] constexpr f32 dot(const Quat& other) const noexcept { return glm::dot(q, other.q); }

        [[nodiscard]] Quat slerp(const Quat& other, f32 t) const noexcept { return {glm::slerp(q, other.q, t)}; }

        [[nodiscard]] Mat4 to_mat4() const noexcept { return {glm::mat4_cast(q)}; }

        // Factory methods
        [[nodiscard]] static constexpr Quat identity() noexcept { return {}; }

        [[nodiscard]] static Quat from_axis_angle(const Vector3& axis, f32 angle) noexcept
        {
            return {glm::angleAxis(angle, axis.v)};
        }

        [[nodiscard]] static Quat from_euler(f32 pitch, f32 yaw, f32 roll) noexcept
        {
            return {glm::quat(glm::vec3(pitch, yaw, roll))};
        }
    };

} // namespace rover
