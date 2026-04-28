#pragma once

#include "core/typedefs.h"

#include <cmath>
#include <limits>

namespace rover {

inline constexpr f64 PI         = 3.14159265358979323846;
inline constexpr f64 TAU        = PI * 2.0;
inline constexpr f64 EPSILON    = 1e-6;
inline constexpr f64 DEG_TO_RAD = PI / 180.0;
inline constexpr f64 RAD_TO_DEG = 180.0 / PI;

template <typename T>
[[nodiscard]] constexpr T lerp(T a, T b, T t) noexcept {
    return a + t * (b - a);
}

template <typename T>
[[nodiscard]] constexpr T clamp(T value, T lo, T hi) noexcept {
    return (value < lo) ? lo : (value > hi) ? hi : value;
}

[[nodiscard]] inline bool is_nearly_equal(f64 a, f64 b,
                                          f64 tolerance = EPSILON) noexcept {
    return std::abs(a - b) <= tolerance;
}

[[nodiscard]] inline bool is_nearly_zero(f64 value,
                                         f64 tolerance = EPSILON) noexcept {
    return std::abs(value) <= tolerance;
}

} // namespace rover
