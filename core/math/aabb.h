#pragma once

#include "core/math/vector3.h"

struct AABB
{
    Vector3 position;
    Vector3 size;

    constexpr AABB() = default;

    constexpr AABB(const Vector3& p_pos, const Vector3& p_size) : position(p_pos), size(p_size) {}

    // -- Queries -------------------------------------------------------------

    [[nodiscard]] Vector3 get_end() const { return position + size; }

    [[nodiscard]] Vector3 get_center() const { return position + size * static_cast<real_t>(0.5); }

    [[nodiscard]] real_t get_volume() const { return size.x * size.y * size.z; }

    [[nodiscard]] bool has_volume() const { return size.x > 0 && size.y > 0 && size.z > 0; }

    [[nodiscard]] bool has_point(const Vector3& p_point) const
    {
        Vector3 end = get_end();
        return p_point.x >= position.x && p_point.x <= end.x && p_point.y >= position.y && p_point.y <= end.y &&
               p_point.z >= position.z && p_point.z <= end.z;
    }

    [[nodiscard]] bool intersects(const AABB& p_aabb) const
    {
        Vector3 end_a = get_end();
        Vector3 end_b = p_aabb.get_end();

        return position.x < end_b.x && end_a.x > p_aabb.position.x && position.y < end_b.y &&
               end_a.y > p_aabb.position.y && position.z < end_b.z && end_a.z > p_aabb.position.z;
    }

    [[nodiscard]] AABB merged(const AABB& p_aabb) const
    {
        Vector3 end_a = get_end();
        Vector3 end_b = p_aabb.get_end();

        Vector3 min_pt = {
            MIN(position.x, p_aabb.position.x),
            MIN(position.y, p_aabb.position.y),
            MIN(position.z, p_aabb.position.z),
        };

        Vector3 max_pt = {
            MAX(end_a.x, end_b.x),
            MAX(end_a.y, end_b.y),
            MAX(end_a.z, end_b.z),
        };

        return {min_pt, max_pt - min_pt};
    }

    void expand_to(const Vector3& p_point)
    {
        Vector3 end = get_end();
        if (p_point.x < position.x)
        {
            position.x = p_point.x;
        }
        if (p_point.y < position.y)
        {
            position.y = p_point.y;
        }
        if (p_point.z < position.z)
        {
            position.z = p_point.z;
        }
        if (p_point.x > end.x)
        {
            end.x = p_point.x;
        }
        if (p_point.y > end.y)
        {
            end.y = p_point.y;
        }
        if (p_point.z > end.z)
        {
            end.z = p_point.z;
        }
        size = end - position;
    }

    // -- Comparison ----------------------------------------------------------

    constexpr bool operator==(const AABB& p_aabb) const { return position == p_aabb.position && size == p_aabb.size; }

    constexpr bool operator!=(const AABB& p_aabb) const { return !(*this == p_aabb); }
};
