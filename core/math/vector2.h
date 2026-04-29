#pragma once

#include "core/math/math_defs.h"
#include "core/typedefs.h"

#include <cmath>

namespace rover
{

    struct Vector2
    {
        struct
        {
            f32 x;
            f32 y;
        } v{};

        constexpr Vector2() noexcept = default;

        constexpr Vector2(f32 x, f32 y) noexcept : v{x, y} {}

        [[nodiscard]] constexpr f32& x() noexcept { return v.x; }

        [[nodiscard]] constexpr f32& y() noexcept { return v.y; }

        [[nodiscard]] constexpr f32 x() const noexcept { return v.x; }

        [[nodiscard]] constexpr f32 y() const noexcept { return v.y; }

        [[nodiscard]] constexpr Vector2 operator+(const Vector2& rhs) const noexcept
        {
            return {v.x + rhs.v.x, v.y + rhs.v.y};
        }

        [[nodiscard]] constexpr Vector2 operator-(const Vector2& rhs) const noexcept
        {
            return {v.x - rhs.v.x, v.y - rhs.v.y};
        }

        [[nodiscard]] constexpr Vector2 operator*(const Vector2& rhs) const noexcept
        {
            return {v.x * rhs.v.x, v.y * rhs.v.y};
        }

        [[nodiscard]] constexpr Vector2 operator/(const Vector2& rhs) const noexcept
        {
            return {v.x / rhs.v.x, v.y / rhs.v.y};
        }

        [[nodiscard]] constexpr Vector2 operator*(f32 s) const noexcept { return {v.x * s, v.y * s}; }

        [[nodiscard]] constexpr Vector2 operator/(f32 s) const noexcept { return {v.x / s, v.y / s}; }

        constexpr Vector2& operator+=(const Vector2& rhs) noexcept
        {
            v.x += rhs.v.x;
            v.y += rhs.v.y;
            return *this;
        }

        constexpr Vector2& operator-=(const Vector2& rhs) noexcept
        {
            v.x -= rhs.v.x;
            v.y -= rhs.v.y;
            return *this;
        }

        constexpr Vector2& operator*=(f32 s) noexcept
        {
            v.x *= s;
            v.y *= s;
            return *this;
        }

        constexpr Vector2& operator/=(f32 s) noexcept
        {
            v.x /= s;
            v.y /= s;
            return *this;
        }

        [[nodiscard]] constexpr Vector2 operator-() const noexcept { return {-v.x, -v.y}; }

        [[nodiscard]] constexpr bool operator==(const Vector2& rhs) const noexcept
        {
            return v.x == rhs.v.x && v.y == rhs.v.y;
        }

        [[nodiscard]] constexpr bool operator!=(const Vector2& rhs) const noexcept { return !(*this == rhs); }

        [[nodiscard]] constexpr f32 length_squared() const noexcept { return v.x * v.x + v.y * v.y; }

        [[nodiscard]] f32 length() const noexcept { return std::sqrt(length_squared()); }

        [[nodiscard]] Vector2 normalized() const noexcept
        {
            const f32 ls = length_squared();
            if (ls <= static_cast<f32>(EPSILON))
            {
                return {};
            }
            const f32 inv = 1.0f / std::sqrt(ls);
            return {v.x * inv, v.y * inv};
        }

        [[nodiscard]] constexpr f32 dot(const Vector2& other) const noexcept
        {
            return v.x * other.v.x + v.y * other.v.y;
        }

        [[nodiscard]] constexpr f32 cross(const Vector2& other) const noexcept
        {
            return v.x * other.v.y - v.y * other.v.x;
        }

        static const Vector2 ZERO;
        static const Vector2 ONE;
        static const Vector2 UP;
        static const Vector2 DOWN;
        static const Vector2 LEFT;
        static const Vector2 RIGHT;
    };

    inline const Vector2 Vector2::ZERO{0.0f, 0.0f};
    inline const Vector2 Vector2::ONE{1.0f, 1.0f};
    inline const Vector2 Vector2::UP{0.0f, 1.0f};
    inline const Vector2 Vector2::DOWN{0.0f, -1.0f};
    inline const Vector2 Vector2::LEFT{-1.0f, 0.0f};
    inline const Vector2 Vector2::RIGHT{1.0f, 0.0f};

    [[nodiscard]] inline constexpr Vector2 operator*(f32 s, const Vector2& v) noexcept
    {
        return v * s;
    }

} // namespace rover
