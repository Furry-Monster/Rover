#pragma once

#include "core/math/vector3.h"

struct Plane
{
    Vector3 normal = {0, 1, 0};
    real_t  d      = 0;

    constexpr Plane() = default;

    constexpr Plane(const Vector3& p_normal, real_t p_d) : normal(p_normal), d(p_d) {}

    Plane(const Vector3& p_normal, const Vector3& p_point) : normal(p_normal), d(p_normal.dot(p_point)) {}

    Plane(const Vector3& p_a, const Vector3& p_b, const Vector3& p_c)
    {
        normal = (p_b - p_a).cross(p_c - p_a).normalized();
        d      = normal.dot(p_a);
    }

    // -- Operations ----------------------------------------------------------

    [[nodiscard]] real_t distance_to(const Vector3& p_point) const { return normal.dot(p_point) - d; }

    [[nodiscard]] bool is_point_over(const Vector3& p_point) const { return distance_to(p_point) > 0; }

    [[nodiscard]] Vector3 project(const Vector3& p_point) const { return p_point - normal * distance_to(p_point); }

    [[nodiscard]] Plane normalized() const
    {
        real_t l = normal.length();
        if (l == 0)
        {
            return {};
        }
        return {normal / l, d / l};
    }

    [[nodiscard]] Vector3 get_center() const { return normal * d; }

    // -- Comparison ----------------------------------------------------------

    constexpr bool operator==(const Plane& p_plane) const { return normal == p_plane.normal && d == p_plane.d; }

    constexpr bool operator!=(const Plane& p_plane) const { return !(*this == p_plane); }
};
