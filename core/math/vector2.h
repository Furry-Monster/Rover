#pragma once

#include "core/typedefs.h"

#include <glm/glm.hpp>

namespace rover
{

    struct Vector2
    {
        glm::vec2 v{0.0f, 0.0f};

        constexpr Vector2() noexcept = default;

        constexpr Vector2(f32 x, f32 y) noexcept : v(x, y) {}

        constexpr Vector2(glm::vec2 vec) noexcept : v(vec) {}

        [[nodiscard]] constexpr f32& x() noexcept { return v.x; }

        [[nodiscard]] constexpr f32& y() noexcept { return v.y; }

        [[nodiscard]] constexpr f32 x() const noexcept { return v.x; }

        [[nodiscard]] constexpr f32 y() const noexcept { return v.y; }

        [[nodiscard]] constexpr operator glm::vec2() const noexcept { return v; }

        // Arithmetic
        [[nodiscard]] constexpr Vector2 operator+(const Vector2& rhs) const noexcept { return {v + rhs.v}; }

        [[nodiscard]] constexpr Vector2 operator-(const Vector2& rhs) const noexcept { return {v - rhs.v}; }

        [[nodiscard]] constexpr Vector2 operator*(const Vector2& rhs) const noexcept { return {v * rhs.v}; }

        [[nodiscard]] constexpr Vector2 operator/(const Vector2& rhs) const noexcept { return {v / rhs.v}; }

        [[nodiscard]] constexpr Vector2 operator*(f32 s) const noexcept { return {v * s}; }

        [[nodiscard]] constexpr Vector2 operator/(f32 s) const noexcept { return {v / s}; }

        constexpr Vector2& operator+=(const Vector2& rhs) noexcept
        {
            v += rhs.v;
            return *this;
        }

        constexpr Vector2& operator-=(const Vector2& rhs) noexcept
        {
            v -= rhs.v;
            return *this;
        }

        constexpr Vector2& operator*=(f32 s) noexcept
        {
            v *= s;
            return *this;
        }

        constexpr Vector2& operator/=(f32 s) noexcept
        {
            v /= s;
            return *this;
        }

        [[nodiscard]] constexpr Vector2 operator-() const noexcept { return {-v}; }

        [[nodiscard]] constexpr bool operator==(const Vector2& rhs) const noexcept { return v == rhs.v; }

        [[nodiscard]] constexpr bool operator!=(const Vector2& rhs) const noexcept { return v != rhs.v; }

        // Geometric operations
        [[nodiscard]] f32 length() const noexcept { return glm::length(v); }

        [[nodiscard]] constexpr f32 length_squared() const noexcept { return v.x * v.x + v.y * v.y; }

        [[nodiscard]] Vector2 normalized() const noexcept { return {glm::normalize(v)}; }

        [[nodiscard]] constexpr f32 dot(const Vector2& other) const noexcept { return glm::dot(v, other.v); }

        [[nodiscard]] constexpr f32 cross(const Vector2& other) const noexcept
        {
            return v.x * other.v.y - v.y * other.v.x;
        }

        // Static constants
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
