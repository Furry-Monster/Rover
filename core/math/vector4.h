#pragma once

#include "core/typedefs.h"

#include <glm/glm.hpp>

namespace rover {

struct Vector4 {
    glm::vec4 v{0.0f, 0.0f, 0.0f, 0.0f};

    constexpr Vector4() noexcept = default;
    constexpr Vector4(f32 x, f32 y, f32 z, f32 w) noexcept : v(x, y, z, w) {}
    constexpr Vector4(glm::vec4 vec) noexcept : v(vec) {}

    [[nodiscard]] constexpr f32& x() noexcept { return v.x; }
    [[nodiscard]] constexpr f32& y() noexcept { return v.y; }
    [[nodiscard]] constexpr f32& z() noexcept { return v.z; }
    [[nodiscard]] constexpr f32& w() noexcept { return v.w; }
    [[nodiscard]] constexpr f32  x() const noexcept { return v.x; }
    [[nodiscard]] constexpr f32  y() const noexcept { return v.y; }
    [[nodiscard]] constexpr f32  z() const noexcept { return v.z; }
    [[nodiscard]] constexpr f32  w() const noexcept { return v.w; }

    [[nodiscard]] constexpr operator glm::vec4() const noexcept { return v; }

    // Arithmetic
    [[nodiscard]] constexpr Vector4 operator+(const Vector4& rhs) const noexcept { return {v + rhs.v}; }
    [[nodiscard]] constexpr Vector4 operator-(const Vector4& rhs) const noexcept { return {v - rhs.v}; }
    [[nodiscard]] constexpr Vector4 operator*(const Vector4& rhs) const noexcept { return {v * rhs.v}; }
    [[nodiscard]] constexpr Vector4 operator/(const Vector4& rhs) const noexcept { return {v / rhs.v}; }
    [[nodiscard]] constexpr Vector4 operator*(f32 s) const noexcept { return {v * s}; }
    [[nodiscard]] constexpr Vector4 operator/(f32 s) const noexcept { return {v / s}; }
    constexpr Vector4& operator+=(const Vector4& rhs) noexcept { v += rhs.v; return *this; }
    constexpr Vector4& operator-=(const Vector4& rhs) noexcept { v -= rhs.v; return *this; }
    constexpr Vector4& operator*=(f32 s) noexcept { v *= s; return *this; }
    constexpr Vector4& operator/=(f32 s) noexcept { v /= s; return *this; }
    [[nodiscard]] constexpr Vector4 operator-() const noexcept { return {-v}; }

    [[nodiscard]] constexpr bool operator==(const Vector4& rhs) const noexcept { return v == rhs.v; }
    [[nodiscard]] constexpr bool operator!=(const Vector4& rhs) const noexcept { return v != rhs.v; }

    // Geometric operations
    [[nodiscard]] f32 length() const noexcept { return glm::length(v); }
    [[nodiscard]] constexpr f32 length_squared() const noexcept { return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w; }
    [[nodiscard]] Vector4 normalized() const noexcept { return {glm::normalize(v)}; }
    [[nodiscard]] constexpr f32 dot(const Vector4& other) const noexcept { return glm::dot(v, other.v); }

    // Static constants
    static const Vector4 ZERO;
    static const Vector4 ONE;
};

inline const Vector4 Vector4::ZERO{0.0f, 0.0f, 0.0f, 0.0f};
inline const Vector4 Vector4::ONE {1.0f, 1.0f, 1.0f, 1.0f};

[[nodiscard]] inline constexpr Vector4 operator*(f32 s, const Vector4& v) noexcept { return v * s; }

} // namespace rover
