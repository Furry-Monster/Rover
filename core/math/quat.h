#pragma once

#include "core/math/mat4.h"
#include "core/math/math_defs.h"
#include "core/math/vector3.h"
#include "core/typedefs.h"

#include <cmath>

namespace rover
{

    /**
     * Orientation quaternion (Godot-style layout: x, y, z, w in `.v`).
     * Legacy factory order remains Quat(scalar_w, x, y, z).
     */
    struct Quat
    {
        struct
        {
            f32 x;
            f32 y;
            f32 z;
            f32 w;
        } v{};

        constexpr Quat() noexcept : v{0.0f, 0.0f, 0.0f, 1.0f} {}

        constexpr Quat(f32 scalar_w, f32 vx, f32 vy, f32 vz) noexcept : v{vx, vy, vz, scalar_w} {}

        [[nodiscard]] constexpr f32& x() noexcept { return v.x; }

        [[nodiscard]] constexpr f32& y() noexcept { return v.y; }

        [[nodiscard]] constexpr f32& z() noexcept { return v.z; }

        [[nodiscard]] constexpr f32& w() noexcept { return v.w; }

        [[nodiscard]] constexpr f32 x() const noexcept { return v.x; }

        [[nodiscard]] constexpr f32 y() const noexcept { return v.y; }

        [[nodiscard]] constexpr f32 z() const noexcept { return v.z; }

        [[nodiscard]] constexpr f32 w() const noexcept { return v.w; }

        [[nodiscard]] constexpr f32 length_squared() const noexcept
        {
            return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
        }

        [[nodiscard]] f32 length() const noexcept { return std::sqrt(length_squared()); }

        [[nodiscard]] Quat normalized() const noexcept
        {
            const f32 ls = length_squared();
            if (ls <= static_cast<f32>(EPSILON))
            {
                return identity();
            }
            const f32 inv = 1.0f / std::sqrt(ls);
            return Quat{v.w * inv, v.x * inv, v.y * inv, v.z * inv};
        }

        [[nodiscard]] Quat inverse() const noexcept
        {
            const f32 ls = length_squared();
            if (ls <= static_cast<f32>(EPSILON))
            {
                return identity();
            }
            const f32 inv = 1.0f / ls;
            return Quat{v.w * inv, -v.x * inv, -v.y * inv, -v.z * inv};
        }

        [[nodiscard]] constexpr f32 dot(const Quat& other) const noexcept
        {
            return v.x * other.v.x + v.y * other.v.y + v.z * other.v.z + v.w * other.v.w;
        }

        [[nodiscard]] Quat operator*(const Quat& rhs) const noexcept
        {
            const f32 qw = v.w * rhs.v.w - v.x * rhs.v.x - v.y * rhs.v.y - v.z * rhs.v.z;
            const f32 qx = v.w * rhs.v.x + v.x * rhs.v.w + v.y * rhs.v.z - v.z * rhs.v.y;
            const f32 qy = v.w * rhs.v.y - v.x * rhs.v.z + v.y * rhs.v.w + v.z * rhs.v.x;
            const f32 qz = v.w * rhs.v.z + v.x * rhs.v.y - v.y * rhs.v.x + v.z * rhs.v.w;
            return Quat{qw, qx, qy, qz};
        }

        Quat& operator*=(const Quat& rhs) noexcept
        {
            *this = *this * rhs;
            return *this;
        }

        [[nodiscard]] constexpr bool operator==(const Quat& rhs) const noexcept
        {
            return v.x == rhs.v.x && v.y == rhs.v.y && v.z == rhs.v.z && v.w == rhs.v.w;
        }

        [[nodiscard]] constexpr bool operator!=(const Quat& rhs) const noexcept { return !(*this == rhs); }

        [[nodiscard]] Vector3 operator*(const Vector3& vec) const noexcept
        {
            const Quat    q = normalized();
            const Vector3 qv{q.v.x, q.v.y, q.v.z};
            const Vector3 uv  = qv.cross(vec);
            const Vector3 uuv = qv.cross(uv);
            return vec + (uv * (2.0f * q.v.w) + uuv * 2.0f);
        }

        [[nodiscard]] Quat slerp(const Quat& other, f32 t) const noexcept
        {
            Quat          a             = *this;
            Quat          b             = other;
            f32           dot           = a.dot(b);
            constexpr f32 DOT_THRESHOLD = 0.9995f;
            if (dot < 0.0f)
            {
                dot   = -dot;
                b.v.x = -b.v.x;
                b.v.y = -b.v.y;
                b.v.z = -b.v.z;
                b.v.w = -b.v.w;
            }
            if (dot > DOT_THRESHOLD)
            {
                const f32 x = a.v.x + t * (b.v.x - a.v.x);
                const f32 y = a.v.y + t * (b.v.y - a.v.y);
                const f32 z = a.v.z + t * (b.v.z - a.v.z);
                const f32 w = a.v.w + t * (b.v.w - a.v.w);
                return Quat{w, x, y, z};
            }

            const f32 theta_0     = std::acos(clamp(dot, -1.0f, 1.0f));
            const f32 theta       = theta_0 * t;
            const f32 sin_theta   = std::sin(theta);
            const f32 sin_theta_0 = std::sin(theta_0);

            const f32 s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
            const f32 s1 = sin_theta / sin_theta_0;

            return Quat{
                s0 * a.v.w + s1 * b.v.w, s0 * a.v.x + s1 * b.v.x, s0 * a.v.y + s1 * b.v.y, s0 * a.v.z + s1 * b.v.z};
        }

        [[nodiscard]] Mat4 to_mat4() const noexcept
        {
            const Quat q = normalized();
            const f32  x = q.v.x;
            const f32  y = q.v.y;
            const f32  z = q.v.z;
            const f32  w = q.v.w;

            const f32 xx = x * x;
            const f32 yy = y * y;
            const f32 zz = z * z;
            const f32 xy = x * y;
            const f32 xz = x * z;
            const f32 yz = y * z;
            const f32 wx = w * x;
            const f32 wy = w * y;
            const f32 wz = w * z;

            Mat4 m{};
            m.c[0][0] = 1.0f - 2.0f * (yy + zz);
            m.c[1][0] = 2.0f * (xy + wz);
            m.c[2][0] = 2.0f * (xz - wy);
            m.c[3][0] = 0.0f;

            m.c[0][1] = 2.0f * (xy - wz);
            m.c[1][1] = 1.0f - 2.0f * (xx + zz);
            m.c[2][1] = 2.0f * (yz + wx);
            m.c[3][1] = 0.0f;

            m.c[0][2] = 2.0f * (xz + wy);
            m.c[1][2] = 2.0f * (yz - wx);
            m.c[2][2] = 1.0f - 2.0f * (xx + yy);
            m.c[3][2] = 0.0f;

            m.c[0][3] = 0.0f;
            m.c[1][3] = 0.0f;
            m.c[2][3] = 0.0f;
            m.c[3][3] = 1.0f;
            return m;
        }

        [[nodiscard]] static constexpr Quat identity() noexcept { return {}; }

        [[nodiscard]] static Quat from_axis_angle(const Vector3& axis, f32 angle) noexcept
        {
            const Vector3 n = axis.normalized();
            const f32     h = angle * 0.5f;
            const f32     s = std::sin(h);
            const f32     c = std::cos(h);
            return Quat{c, n.v.x * s, n.v.y * s, n.v.z * s};
        }

        [[nodiscard]] static Quat from_euler(f32 pitch, f32 yaw, f32 roll) noexcept
        {
            const Quat qx = from_axis_angle(Vector3::RIGHT, pitch);
            const Quat qy = from_axis_angle(Vector3::UP, yaw);
            const Quat qz = from_axis_angle(Vector3::FORWARD, roll);
            return (qy * qx * qz).normalized();
        }
    };

} // namespace rover
