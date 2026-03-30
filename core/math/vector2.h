#pragma once

#include "core/math/math_defs.h"

struct Vector2
{
    real_t x = 0;
    real_t y = 0;

    constexpr Vector2() = default;

    constexpr Vector2(real_t p_x, real_t p_y) : x(p_x), y(p_y) {}

    // -- Arithmetic ----------------------------------------------------------

    constexpr Vector2 operator+(const Vector2& p_v) const { return {x + p_v.x, y + p_v.y}; }

    constexpr Vector2 operator-(const Vector2& p_v) const { return {x - p_v.x, y - p_v.y}; }

    constexpr Vector2 operator*(const Vector2& p_v) const { return {x * p_v.x, y * p_v.y}; }

    constexpr Vector2 operator/(const Vector2& p_v) const { return {x / p_v.x, y / p_v.y}; }

    constexpr Vector2 operator*(real_t p_s) const { return {x * p_s, y * p_s}; }

    constexpr Vector2 operator/(real_t p_s) const { return {x / p_s, y / p_s}; }

    constexpr Vector2 operator-() const { return {-x, -y}; }

    Vector2& operator+=(const Vector2& p_v)
    {
        x += p_v.x;
        y += p_v.y;
        return *this;
    }

    Vector2& operator-=(const Vector2& p_v)
    {
        x -= p_v.x;
        y -= p_v.y;
        return *this;
    }

    Vector2& operator*=(real_t p_s)
    {
        x *= p_s;
        y *= p_s;
        return *this;
    }

    Vector2& operator/=(real_t p_s)
    {
        x /= p_s;
        y /= p_s;
        return *this;
    }

    // -- Comparison ----------------------------------------------------------

    constexpr bool operator==(const Vector2& p_v) const { return x == p_v.x && y == p_v.y; }

    constexpr bool operator!=(const Vector2& p_v) const { return !(*this == p_v); }

    // -- Access --------------------------------------------------------------

    real_t& operator[](int p_idx) { return p_idx == 0 ? x : y; }

    const real_t& operator[](int p_idx) const { return p_idx == 0 ? x : y; }

    // -- Operations ----------------------------------------------------------

    [[nodiscard]] real_t length_squared() const { return x * x + y * y; }

    [[nodiscard]] real_t length() const { return std::sqrt(length_squared()); }

    [[nodiscard]] real_t dot(const Vector2& p_v) const { return x * p_v.x + y * p_v.y; }

    [[nodiscard]] real_t cross(const Vector2& p_v) const { return x * p_v.y - y * p_v.x; }

    [[nodiscard]] real_t distance_to(const Vector2& p_v) const { return (*this - p_v).length(); }

    [[nodiscard]] real_t angle() const { return std::atan2(y, x); }

    [[nodiscard]] real_t angle_to(const Vector2& p_v) const { return std::atan2(cross(p_v), dot(p_v)); }

    [[nodiscard]] Vector2 abs() const { return {std::abs(x), std::abs(y)}; }

    [[nodiscard]] Vector2 lerp(const Vector2& p_to, real_t p_t) const { return *this + (p_to - *this) * p_t; }

    [[nodiscard]] bool is_equal_approx(const Vector2& p_v) const
    {
        return math_is_equal_approx(x, p_v.x) && math_is_equal_approx(y, p_v.y);
    }

    [[nodiscard]] Vector2 normalized() const
    {
        real_t l = length();
        return l > 0 ? *this / l : Vector2();
    }

    // -- Constants -----------------------------------------------------------

    static constexpr Vector2 ZERO() { return {0, 0}; }

    static constexpr Vector2 ONE() { return {1, 1}; }

    static constexpr Vector2 UP() { return {0, 1}; }

    static constexpr Vector2 DOWN() { return {0, -1}; }

    static constexpr Vector2 LEFT() { return {-1, 0}; }

    static constexpr Vector2 RIGHT() { return {1, 0}; }
};

inline constexpr Vector2
operator*(real_t p_s, const Vector2& p_v)
{
    return p_v * p_s;
}
