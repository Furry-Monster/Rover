#pragma once

#include "core/typedefs.h"

#include <glm/glm.hpp>

namespace rover {

struct Vector3 {
    glm::vec3 v{0.0f, 0.0f, 0.0f};

    constexpr Vector3() noexcept = default;
    constexpr Vector3(f32 x, f32 y, f32 z) noexcept : v(x, y, z) {}
    constexpr Vector3(glm::vec3 vec) noexcept : v(vec) {}

    [[nodiscard]] constexpr f32& x() noexcept { return v.x; }
    [[nodiscard]] constexpr f32& y() noexcept { return v.y; }
    [[nodiscard]] constexpr f32& z() noexcept { return v.z; }
    [[nodiscard]] constexpr f32  x() const noexcept { return v.x; }
    [[nodiscard]] constexpr f32  y() const noexcept { return v.y; }
    [[nodiscard]] constexpr f32  z() const noexcept { return v.z; }

    [[nodiscard]] constexpr operator glm::vec3() const noexcept { return v; }

    // Arithmetic
    [[nodiscard]] constexpr Vector3 operator+(const Vector3& rhs) const noexcept { return {v + rhs.v}; }
    [[nodiscard]] constexpr Vector3 operator-(const Vector3& rhs) const noexcept { return {v - rhs.v}; }
    [[nodiscard]] constexpr Vector3 operator*(const Vector3& rhs) const noexcept { return {v * rhs.v}; }
    [[nodiscard]] constexpr Vector3 operator/(const Vector3& rhs) const noexcept { return {v / rhs.v}; }
    [[nodiscard]] constexpr Vector3 operator*(f32 s) const noexcept { return {v * s}; }
    [[nodiscard]] constexpr Vector3 operator/(f32 s) const noexcept { return {v / s}; }
    constexpr Vector3& operator+=(const Vector3& rhs) noexcept { v += rhs.v; return *this; }
    constexpr Vector3& operator-=(const Vector3& rhs) noexcept { v -= rhs.v; return *this; }
    constexpr Vector3& operator*=(f32 s) noexcept { v *= s; return *this; }
    constexpr Vector3& operator/=(f32 s) noexcept { v /= s; return *this; }
    [[nodiscard]] constexpr Vector3 operator-() const noexcept { return {-v}; }

    [[nodiscard]] constexpr bool operator==(const Vector3& rhs) const noexcept { return v == rhs.v; }
    [[nodiscard]] constexpr bool operator!=(const Vector3& rhs) const noexcept { return v != rhs.v; }

    // Geometric operations
    [[nodiscard]] f32 length() const noexcept { return glm::length(v); }
    [[nodiscard]] constexpr f32 length_squared() const noexcept { return v.x * v.x + v.y * v.y + v.z * v.z; }
    [[nodiscard]] Vector3 normalized() const noexcept { return {glm::normalize(v)}; }
    [[nodiscard]] constexpr f32 dot(const Vector3& other) const noexcept { return glm::dot(v, other.v); }
    [[nodiscard]] constexpr Vector3 cross(const Vector3& other) const noexcept { return {glm::cross(v, other.v)}; }

    // Static constants
    static const Vector3 ZERO;
    static const Vector3 ONE;
    static const Vector3 UP;
    static const Vector3 DOWN;
    static const Vector3 LEFT;
    static const Vector3 RIGHT;
    static const Vector3 FORWARD;
    static const Vector3 BACK;
};

inline const Vector3 Vector3::ZERO    { 0.0f,  0.0f,  0.0f};
inline const Vector3 Vector3::ONE     { 1.0f,  1.0f,  1.0f};
inline const Vector3 Vector3::UP      { 0.0f,  1.0f,  0.0f};
inline const Vector3 Vector3::DOWN    { 0.0f, -1.0f,  0.0f};
inline const Vector3 Vector3::LEFT    {-1.0f,  0.0f,  0.0f};
inline const Vector3 Vector3::RIGHT   { 1.0f,  0.0f,  0.0f};
inline const Vector3 Vector3::FORWARD { 0.0f,  0.0f, -1.0f};
inline const Vector3 Vector3::BACK    { 0.0f,  0.0f,  1.0f};

[[nodiscard]] inline constexpr Vector3 operator*(f32 s, const Vector3& v) noexcept { return v * s; }

} // namespace rover
