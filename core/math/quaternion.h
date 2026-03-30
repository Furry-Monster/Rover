#pragma once

#include "core/math/vector3.h"

struct Quaternion
{
    real_t x = 0;
    real_t y = 0;
    real_t z = 0;
    real_t w = 1;

    constexpr Quaternion() = default;

    constexpr Quaternion(real_t p_x, real_t p_y, real_t p_z, real_t p_w) : x(p_x), y(p_y), z(p_z), w(p_w) {}

    Quaternion(const Vector3& p_axis, real_t p_angle)
    {
        real_t half = p_angle * static_cast<real_t>(0.5);
        real_t s    = std::sin(half);
        x           = p_axis.x * s;
        y           = p_axis.y * s;
        z           = p_axis.z * s;
        w           = std::cos(half);
    }

    // -- Arithmetic ----------------------------------------------------------

    Quaternion operator*(const Quaternion& p_q) const
    {
        return {
            w * p_q.x + x * p_q.w + y * p_q.z - z * p_q.y,
            w * p_q.y - x * p_q.z + y * p_q.w + z * p_q.x,
            w * p_q.z + x * p_q.y - y * p_q.x + z * p_q.w,
            w * p_q.w - x * p_q.x - y * p_q.y - z * p_q.z,
        };
    }

    Vector3 operator*(const Vector3& p_v) const
    {
        Vector3 u(x, y, z);
        Vector3 uv  = u.cross(p_v);
        Vector3 uuv = u.cross(uv);
        return p_v + (uv * w + uuv) * static_cast<real_t>(2);
    }

    constexpr Quaternion operator-() const { return {-x, -y, -z, -w}; }

    // -- Comparison ----------------------------------------------------------

    constexpr bool operator==(const Quaternion& p_q) const
    {
        return x == p_q.x && y == p_q.y && z == p_q.z && w == p_q.w;
    }

    constexpr bool operator!=(const Quaternion& p_q) const { return !(*this == p_q); }

    // -- Operations ----------------------------------------------------------

    [[nodiscard]] real_t length_squared() const { return x * x + y * y + z * z + w * w; }

    [[nodiscard]] real_t length() const { return std::sqrt(length_squared()); }

    [[nodiscard]] real_t dot(const Quaternion& p_q) const { return x * p_q.x + y * p_q.y + z * p_q.z + w * p_q.w; }

    [[nodiscard]] Quaternion normalized() const
    {
        real_t l = length();
        if (l == 0)
        {
            return {};
        }
        return {x / l, y / l, z / l, w / l};
    }

    [[nodiscard]] Quaternion inverse() const
    {
        real_t l2 = length_squared();
        if (l2 == 0)
        {
            return {};
        }
        return {-x / l2, -y / l2, -z / l2, w / l2};
    }

    [[nodiscard]] bool is_equal_approx(const Quaternion& p_q) const
    {
        return math_is_equal_approx(x, p_q.x) && math_is_equal_approx(y, p_q.y) && math_is_equal_approx(z, p_q.z) &&
               math_is_equal_approx(w, p_q.w);
    }

    [[nodiscard]] static Quaternion slerp(const Quaternion& p_from, const Quaternion& p_to, real_t p_t)
    {
        real_t     d  = p_from.dot(p_to);
        Quaternion to = p_to;

        if (d < 0)
        {
            d  = -d;
            to = -to;
        }

        if (d > static_cast<real_t>(0.9995))
        {
            return {
                math_lerp(p_from.x, to.x, p_t),
                math_lerp(p_from.y, to.y, p_t),
                math_lerp(p_from.z, to.z, p_t),
                math_lerp(p_from.w, to.w, p_t),
            };
        }

        real_t theta  = std::acos(d);
        real_t sin_t  = std::sin(theta);
        real_t s_from = std::sin((1 - p_t) * theta) / sin_t;
        real_t s_to   = std::sin(p_t * theta) / sin_t;

        return {
            s_from * p_from.x + s_to * to.x,
            s_from * p_from.y + s_to * to.y,
            s_from * p_from.z + s_to * to.z,
            s_from * p_from.w + s_to * to.w,
        };
    }

    // -- Constants -----------------------------------------------------------

    static constexpr Quaternion IDENTITY() { return {0, 0, 0, 1}; }
};
