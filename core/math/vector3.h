#pragma once

#include "core/math/math_defs.h"
#include "core/typedefs.h"

#include <cmath>

namespace rover
{

    /** Right-handed 3D vector; storage matches historical `.v.x` layout. */
    struct Vector3
    {
        struct
        {
            f32 x;
            f32 y;
            f32 z;
        } v{};

        constexpr Vector3() noexcept = default;

        constexpr Vector3(f32 x, f32 y, f32 z) noexcept : v{x, y, z} {}

        [[nodiscard]] constexpr f32& x() noexcept { return v.x; }

        [[nodiscard]] constexpr f32& y() noexcept { return v.y; }

        [[nodiscard]] constexpr f32& z() noexcept { return v.z; }

        [[nodiscard]] constexpr f32 x() const noexcept { return v.x; }

        [[nodiscard]] constexpr f32 y() const noexcept { return v.y; }

        [[nodiscard]] constexpr f32 z() const noexcept { return v.z; }

        [[nodiscard]] constexpr Vector3 operator+(const Vector3& rhs) const noexcept
        {
            return {v.x + rhs.v.x, v.y + rhs.v.y, v.z + rhs.v.z};
        }

        [[nodiscard]] constexpr Vector3 operator-(const Vector3& rhs) const noexcept
        {
            return {v.x - rhs.v.x, v.y - rhs.v.y, v.z - rhs.v.z};
        }

        [[nodiscard]] constexpr Vector3 operator*(const Vector3& rhs) const noexcept
        {
            return {v.x * rhs.v.x, v.y * rhs.v.y, v.z * rhs.v.z};
        }

        [[nodiscard]] constexpr Vector3 operator/(const Vector3& rhs) const noexcept
        {
            return {v.x / rhs.v.x, v.y / rhs.v.y, v.z / rhs.v.z};
        }

        [[nodiscard]] constexpr Vector3 operator*(f32 s) const noexcept { return {v.x * s, v.y * s, v.z * s}; }

        [[nodiscard]] constexpr Vector3 operator/(f32 s) const noexcept { return {v.x / s, v.y / s, v.z / s}; }

        constexpr Vector3& operator+=(const Vector3& rhs) noexcept
        {
            v.x += rhs.v.x;
            v.y += rhs.v.y;
            v.z += rhs.v.z;
            return *this;
        }

        constexpr Vector3& operator-=(const Vector3& rhs) noexcept
        {
            v.x -= rhs.v.x;
            v.y -= rhs.v.y;
            v.z -= rhs.v.z;
            return *this;
        }

        constexpr Vector3& operator*=(f32 s) noexcept
        {
            v.x *= s;
            v.y *= s;
            v.z *= s;
            return *this;
        }

        constexpr Vector3& operator/=(f32 s) noexcept
        {
            v.x /= s;
            v.y /= s;
            v.z /= s;
            return *this;
        }

        [[nodiscard]] constexpr Vector3 operator-() const noexcept { return {-v.x, -v.y, -v.z}; }

        [[nodiscard]] constexpr bool operator==(const Vector3& rhs) const noexcept
        {
            return v.x == rhs.v.x && v.y == rhs.v.y && v.z == rhs.v.z;
        }

        [[nodiscard]] constexpr bool operator!=(const Vector3& rhs) const noexcept { return !(*this == rhs); }

        [[nodiscard]] constexpr f32 length_squared() const noexcept { return v.x * v.x + v.y * v.y + v.z * v.z; }

        [[nodiscard]] f32 length() const noexcept { return std::sqrt(length_squared()); }

        [[nodiscard]] Vector3 normalized() const noexcept
        {
            const f32 ls = length_squared();
            if (ls <= static_cast<f32>(EPSILON))
            {
                return {};
            }
            const f32 inv = 1.0f / std::sqrt(ls);
            return {v.x * inv, v.y * inv, v.z * inv};
        }

        [[nodiscard]] constexpr f32 dot(const Vector3& other) const noexcept
        {
            return v.x * other.v.x + v.y * other.v.y + v.z * other.v.z;
        }

        [[nodiscard]] constexpr Vector3 cross(const Vector3& other) const noexcept
        {
            return {v.y * other.v.z - v.z * other.v.y,
                    v.z * other.v.x - v.x * other.v.z,
                    v.x * other.v.y - v.y * other.v.x};
        }

        static const Vector3 ZERO;
        static const Vector3 ONE;
        static const Vector3 UP;
        static const Vector3 DOWN;
        static const Vector3 LEFT;
        static const Vector3 RIGHT;
        static const Vector3 FORWARD;
        static const Vector3 BACK;
    };

    inline const Vector3 Vector3::ZERO{0.0f, 0.0f, 0.0f};
    inline const Vector3 Vector3::ONE{1.0f, 1.0f, 1.0f};
    inline const Vector3 Vector3::UP{0.0f, 1.0f, 0.0f};
    inline const Vector3 Vector3::DOWN{0.0f, -1.0f, 0.0f};
    inline const Vector3 Vector3::LEFT{-1.0f, 0.0f, 0.0f};
    inline const Vector3 Vector3::RIGHT{1.0f, 0.0f, 0.0f};
    inline const Vector3 Vector3::FORWARD{0.0f, 0.0f, -1.0f};
    inline const Vector3 Vector3::BACK{0.0f, 0.0f, 1.0f};

    [[nodiscard]] inline constexpr Vector3 operator*(f32 s, const Vector3& v) noexcept
    {
        return v * s;
    }

} // namespace rover
