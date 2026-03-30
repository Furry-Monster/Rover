#pragma once

#include "core/math/math_defs.h"

struct Vector4
{
    real_t x = 0;
    real_t y = 0;
    real_t z = 0;
    real_t w = 0;

    constexpr Vector4() = default;

    constexpr Vector4(real_t p_x, real_t p_y, real_t p_z, real_t p_w) : x(p_x), y(p_y), z(p_z), w(p_w) {}

    // -- Arithmetic ----------------------------------------------------------

    constexpr Vector4 operator+(const Vector4& p_v) const { return {x + p_v.x, y + p_v.y, z + p_v.z, w + p_v.w}; }

    constexpr Vector4 operator-(const Vector4& p_v) const { return {x - p_v.x, y - p_v.y, z - p_v.z, w - p_v.w}; }

    constexpr Vector4 operator*(real_t p_s) const { return {x * p_s, y * p_s, z * p_s, w * p_s}; }

    constexpr Vector4 operator/(real_t p_s) const { return {x / p_s, y / p_s, z / p_s, w / p_s}; }

    constexpr Vector4 operator-() const { return {-x, -y, -z, -w}; }

    Vector4& operator+=(const Vector4& p_v)
    {
        x += p_v.x;
        y += p_v.y;
        z += p_v.z;
        w += p_v.w;
        return *this;
    }

    Vector4& operator-=(const Vector4& p_v)
    {
        x -= p_v.x;
        y -= p_v.y;
        z -= p_v.z;
        w -= p_v.w;
        return *this;
    }

    Vector4& operator*=(real_t p_s)
    {
        x *= p_s;
        y *= p_s;
        z *= p_s;
        w *= p_s;
        return *this;
    }

    Vector4& operator/=(real_t p_s)
    {
        x /= p_s;
        y /= p_s;
        z /= p_s;
        w /= p_s;
        return *this;
    }

    // -- Comparison ----------------------------------------------------------

    constexpr bool operator==(const Vector4& p_v) const { return x == p_v.x && y == p_v.y && z == p_v.z && w == p_v.w; }

    constexpr bool operator!=(const Vector4& p_v) const { return !(*this == p_v); }

    // -- Access --------------------------------------------------------------

    real_t& operator[](int p_idx)
    {
        switch (p_idx)
        {
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            default:
                return w;
        }
    }

    const real_t& operator[](int p_idx) const
    {
        switch (p_idx)
        {
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            default:
                return w;
        }
    }

    // -- Operations ----------------------------------------------------------

    [[nodiscard]] real_t length_squared() const { return x * x + y * y + z * z + w * w; }

    [[nodiscard]] real_t length() const { return std::sqrt(length_squared()); }

    [[nodiscard]] real_t dot(const Vector4& p_v) const { return x * p_v.x + y * p_v.y + z * p_v.z + w * p_v.w; }

    [[nodiscard]] Vector4 lerp(const Vector4& p_to, real_t p_t) const { return *this + (p_to - *this) * p_t; }

    [[nodiscard]] bool is_equal_approx(const Vector4& p_v) const
    {
        return math_is_equal_approx(x, p_v.x) && math_is_equal_approx(y, p_v.y) && math_is_equal_approx(z, p_v.z) &&
               math_is_equal_approx(w, p_v.w);
    }

    [[nodiscard]] Vector4 normalized() const
    {
        real_t l = length();
        return l > 0 ? *this / l : Vector4();
    }
};

inline constexpr Vector4
operator*(real_t p_s, const Vector4& p_v)
{
    return p_v * p_s;
}
