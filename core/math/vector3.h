#pragma once

#include "core/math/math_defs.h"

struct Vector3
{
    real_t x = 0;
    real_t y = 0;
    real_t z = 0;

    constexpr Vector3() = default;

    constexpr Vector3(real_t p_x, real_t p_y, real_t p_z) : x(p_x), y(p_y), z(p_z) {}

    // -- Arithmetic ----------------------------------------------------------

    constexpr Vector3 operator+(const Vector3& p_v) const { return {x + p_v.x, y + p_v.y, z + p_v.z}; }

    constexpr Vector3 operator-(const Vector3& p_v) const { return {x - p_v.x, y - p_v.y, z - p_v.z}; }

    constexpr Vector3 operator*(const Vector3& p_v) const { return {x * p_v.x, y * p_v.y, z * p_v.z}; }

    constexpr Vector3 operator/(const Vector3& p_v) const { return {x / p_v.x, y / p_v.y, z / p_v.z}; }

    constexpr Vector3 operator*(real_t p_s) const { return {x * p_s, y * p_s, z * p_s}; }

    constexpr Vector3 operator/(real_t p_s) const { return {x / p_s, y / p_s, z / p_s}; }

    constexpr Vector3 operator-() const { return {-x, -y, -z}; }

    Vector3& operator+=(const Vector3& p_v)
    {
        x += p_v.x;
        y += p_v.y;
        z += p_v.z;
        return *this;
    }

    Vector3& operator-=(const Vector3& p_v)
    {
        x -= p_v.x;
        y -= p_v.y;
        z -= p_v.z;
        return *this;
    }

    Vector3& operator*=(real_t p_s)
    {
        x *= p_s;
        y *= p_s;
        z *= p_s;
        return *this;
    }

    Vector3& operator/=(real_t p_s)
    {
        x /= p_s;
        y /= p_s;
        z /= p_s;
        return *this;
    }

    // -- Comparison ----------------------------------------------------------

    constexpr bool operator==(const Vector3& p_v) const { return x == p_v.x && y == p_v.y && z == p_v.z; }

    constexpr bool operator!=(const Vector3& p_v) const { return !(*this == p_v); }

    // -- Access --------------------------------------------------------------

    real_t& operator[](int p_idx) { return p_idx == 0 ? x : (p_idx == 1 ? y : z); }

    const real_t& operator[](int p_idx) const { return p_idx == 0 ? x : (p_idx == 1 ? y : z); }

    // -- Operations ----------------------------------------------------------

    [[nodiscard]] real_t length_squared() const { return x * x + y * y + z * z; }

    [[nodiscard]] real_t length() const { return std::sqrt(length_squared()); }

    [[nodiscard]] real_t dot(const Vector3& p_v) const { return x * p_v.x + y * p_v.y + z * p_v.z; }

    [[nodiscard]] Vector3 cross(const Vector3& p_v) const
    {
        return {
            y * p_v.z - z * p_v.y,
            z * p_v.x - x * p_v.z,
            x * p_v.y - y * p_v.x,
        };
    }

    [[nodiscard]] real_t distance_to(const Vector3& p_v) const { return (*this - p_v).length(); }

    [[nodiscard]] Vector3 abs() const { return {std::abs(x), std::abs(y), std::abs(z)}; }

    [[nodiscard]] Vector3 lerp(const Vector3& p_to, real_t p_t) const { return *this + (p_to - *this) * p_t; }

    [[nodiscard]] bool is_equal_approx(const Vector3& p_v) const
    {
        return math_is_equal_approx(x, p_v.x) && math_is_equal_approx(y, p_v.y) && math_is_equal_approx(z, p_v.z);
    }

    [[nodiscard]] Vector3 normalized() const
    {
        real_t l = length();
        return l > 0 ? *this / l : Vector3();
    }

    // -- Constants -----------------------------------------------------------

    static constexpr Vector3 ZERO() { return {0, 0, 0}; }

    static constexpr Vector3 ONE() { return {1, 1, 1}; }

    static constexpr Vector3 UP() { return {0, 1, 0}; }

    static constexpr Vector3 DOWN() { return {0, -1, 0}; }

    static constexpr Vector3 LEFT() { return {-1, 0, 0}; }

    static constexpr Vector3 RIGHT() { return {1, 0, 0}; }

    static constexpr Vector3 FORWARD() { return {0, 0, -1}; }

    static constexpr Vector3 BACK() { return {0, 0, 1}; }
};

inline constexpr Vector3
operator*(real_t p_s, const Vector3& p_v)
{
    return p_v * p_s;
}
