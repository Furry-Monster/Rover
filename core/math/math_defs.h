#pragma once

#include "core/typedefs.h"

#include <cmath>
#include <limits>

inline constexpr real_t MATH_PI      = static_cast<real_t>(3.14159265358979323846);
inline constexpr real_t MATH_TAU     = static_cast<real_t>(6.28318530717958647692);
inline constexpr real_t MATH_HALF_PI = static_cast<real_t>(1.57079632679489661923);
inline constexpr real_t MATH_E       = static_cast<real_t>(2.71828182845904523536);
inline constexpr real_t MATH_SQRT2   = static_cast<real_t>(1.41421356237309504880);

inline constexpr real_t CMP_EPSILON = static_cast<real_t>(0.00001);

[[nodiscard]] inline constexpr real_t
math_deg_to_rad(real_t p_deg) noexcept
{
    return p_deg * (MATH_PI / static_cast<real_t>(180.0));
}

[[nodiscard]] inline constexpr real_t
math_rad_to_deg(real_t p_rad) noexcept
{
    return p_rad * (static_cast<real_t>(180.0) / MATH_PI);
}

[[nodiscard]] inline constexpr real_t
math_lerp(real_t p_from, real_t p_to, real_t p_weight) noexcept
{
    return p_from + (p_to - p_from) * p_weight;
}

[[nodiscard]] inline bool
math_is_equal_approx(real_t a, real_t b, real_t p_tolerance = CMP_EPSILON) noexcept
{
    return std::abs(a - b) < p_tolerance;
}

[[nodiscard]] inline bool
math_is_zero_approx(real_t a, real_t p_tolerance = CMP_EPSILON) noexcept
{
    return std::abs(a) < p_tolerance;
}
