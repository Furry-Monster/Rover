#pragma once

#include "core/math/math_defs.h"
#include "core/typedefs.h"

#include <cmath>

namespace rover
{

    struct Vector4
    {
        struct
        {
            f32 x;
            f32 y;
            f32 z;
            f32 w;
        } v{};

        constexpr Vector4() noexcept = default;

        constexpr Vector4(f32 x, f32 y, f32 z, f32 w) noexcept : v{x, y, z, w} {}

        [[nodiscard]] constexpr f32& x() noexcept { return v.x; }

        [[nodiscard]] constexpr f32& y() noexcept { return v.y; }

        [[nodiscard]] constexpr f32& z() noexcept { return v.z; }

        [[nodiscard]] constexpr f32& w() noexcept { return v.w; }

        [[nodiscard]] constexpr f32 x() const noexcept { return v.x; }

        [[nodiscard]] constexpr f32 y() const noexcept { return v.y; }

        [[nodiscard]] constexpr f32 z() const noexcept { return v.z; }

        [[nodiscard]] constexpr f32 w() const noexcept { return v.w; }

        [[nodiscard]] constexpr f32 operator[](int idx) const noexcept
        {
            return idx == 0 ? v.x : idx == 1 ? v.y : idx == 2 ? v.z : v.w;
        }

        [[nodiscard]] constexpr f32& operator[](int idx) noexcept
        {
            return idx == 0 ? v.x : idx == 1 ? v.y : idx == 2 ? v.z : v.w;
        }

        [[nodiscard]] constexpr Vector4 operator+(const Vector4& rhs) const noexcept
        {
            return {
                v.x + rhs.v.x,
                v.y + rhs.v.y,
                v.z + rhs.v.z,
                v.w + rhs.v.w,
            };
        }

        [[nodiscard]] constexpr Vector4 operator-(const Vector4& rhs) const noexcept
        {
            return {
                v.x - rhs.v.x,
                v.y - rhs.v.y,
                v.z - rhs.v.z,
                v.w - rhs.v.w,
            };
        }

        [[nodiscard]] constexpr Vector4 operator*(const Vector4& rhs) const noexcept
        {
            return {
                v.x * rhs.v.x,
                v.y * rhs.v.y,
                v.z * rhs.v.z,
                v.w * rhs.v.w,
            };
        }

        [[nodiscard]] constexpr Vector4 operator/(const Vector4& rhs) const noexcept
        {
            return {
                v.x / rhs.v.x,
                v.y / rhs.v.y,
                v.z / rhs.v.z,
                v.w / rhs.v.w,
            };
        }

        [[nodiscard]] constexpr Vector4 operator*(f32 s) const noexcept { return {v.x * s, v.y * s, v.z * s, v.w * s}; }

        [[nodiscard]] constexpr Vector4 operator/(f32 s) const noexcept { return {v.x / s, v.y / s, v.z / s, v.w / s}; }

        constexpr Vector4& operator+=(const Vector4& rhs) noexcept
        {
            v.x += rhs.v.x;
            v.y += rhs.v.y;
            v.z += rhs.v.z;
            v.w += rhs.v.w;
            return *this;
        }

        constexpr Vector4& operator-=(const Vector4& rhs) noexcept
        {
            v.x -= rhs.v.x;
            v.y -= rhs.v.y;
            v.z -= rhs.v.z;
            v.w -= rhs.v.w;
            return *this;
        }

        constexpr Vector4& operator*=(f32 s) noexcept
        {
            v.x *= s;
            v.y *= s;
            v.z *= s;
            v.w *= s;
            return *this;
        }

        constexpr Vector4& operator/=(f32 s) noexcept
        {
            v.x /= s;
            v.y /= s;
            v.z /= s;
            v.w /= s;
            return *this;
        }

        [[nodiscard]] constexpr Vector4 operator-() const noexcept { return {-v.x, -v.y, -v.z, -v.w}; }

        [[nodiscard]] constexpr bool operator==(const Vector4& rhs) const noexcept
        {
            return v.x == rhs.v.x && v.y == rhs.v.y && v.z == rhs.v.z && v.w == rhs.v.w;
        }

        [[nodiscard]] constexpr bool operator!=(const Vector4& rhs) const noexcept { return !(*this == rhs); }

        [[nodiscard]] constexpr f32 length_squared() const noexcept
        {
            return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
        }

        [[nodiscard]] f32 length() const noexcept { return std::sqrt(length_squared()); }

        [[nodiscard]] Vector4 normalized() const noexcept
        {
            const f32 ls = length_squared();
            if (ls <= static_cast<f32>(EPSILON))
            {
                return {};
            }
            const f32 inv = 1.0f / std::sqrt(ls);
            return {v.x * inv, v.y * inv, v.z * inv, v.w * inv};
        }

        [[nodiscard]] constexpr f32 dot(const Vector4& other) const noexcept
        {
            return v.x * other.v.x + v.y * other.v.y + v.z * other.v.z + v.w * other.v.w;
        }

        static const Vector4 ZERO;
        static const Vector4 ONE;
    };

    inline const Vector4 Vector4::ZERO{0.0f, 0.0f, 0.0f, 0.0f};
    inline const Vector4 Vector4::ONE{1.0f, 1.0f, 1.0f, 1.0f};

    [[nodiscard]] inline constexpr Vector4 operator*(f32 s, const Vector4& v) noexcept
    {
        return v * s;
    }

} // namespace rover
