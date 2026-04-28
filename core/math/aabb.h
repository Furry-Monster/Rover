#pragma once

#include "core/typedefs.h"
#include "core/math/vector3.h"

#include <algorithm>

namespace rover {

struct AABB {
    Vector3 min{};
    Vector3 max{};

    constexpr AABB() noexcept = default;
    constexpr AABB(const Vector3& min, const Vector3& max) noexcept : min(min), max(max) {}

    [[nodiscard]] constexpr Vector3 get_center() const noexcept {
        return (min + max) * 0.5f;
    }

    [[nodiscard]] constexpr Vector3 get_size() const noexcept {
        return max - min;
    }

    [[nodiscard]] constexpr bool contains(const Vector3& point) const noexcept {
        return point.v.x >= min.v.x && point.v.x <= max.v.x
            && point.v.y >= min.v.y && point.v.y <= max.v.y
            && point.v.z >= min.v.z && point.v.z <= max.v.z;
    }

    [[nodiscard]] constexpr bool intersects(const AABB& other) const noexcept {
        return min.v.x <= other.max.v.x && max.v.x >= other.min.v.x
            && min.v.y <= other.max.v.y && max.v.y >= other.min.v.y
            && min.v.z <= other.max.v.z && max.v.z >= other.min.v.z;
    }

    [[nodiscard]] constexpr AABB merge(const AABB& other) const noexcept {
        return {
            {std::min(min.v.x, other.min.v.x),
             std::min(min.v.y, other.min.v.y),
             std::min(min.v.z, other.min.v.z)},
            {std::max(max.v.x, other.max.v.x),
             std::max(max.v.y, other.max.v.y),
             std::max(max.v.z, other.max.v.z)}
        };
    }

    constexpr void expand(const Vector3& point) noexcept {
        min = {std::min(min.v.x, point.v.x),
               std::min(min.v.y, point.v.y),
               std::min(min.v.z, point.v.z)};
        max = {std::max(max.v.x, point.v.x),
               std::max(max.v.y, point.v.y),
               std::max(max.v.z, point.v.z)};
    }

    [[nodiscard]] constexpr bool operator==(const AABB& rhs) const noexcept {
        return min == rhs.min && max == rhs.max;
    }
    [[nodiscard]] constexpr bool operator!=(const AABB& rhs) const noexcept {
        return !(*this == rhs);
    }
};

} // namespace rover
