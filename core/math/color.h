#pragma once

#include "core/math/math_defs.h"

#include <algorithm>

struct Color
{
    real_t r = 0;
    real_t g = 0;
    real_t b = 0;
    real_t a = 1;

    constexpr Color() = default;

    constexpr Color(real_t p_r, real_t p_g, real_t p_b, real_t p_a = 1) : r(p_r), g(p_g), b(p_b), a(p_a) {}

    // -- Arithmetic ----------------------------------------------------------

    constexpr Color operator+(const Color& p_c) const { return {r + p_c.r, g + p_c.g, b + p_c.b, a + p_c.a}; }

    constexpr Color operator-(const Color& p_c) const { return {r - p_c.r, g - p_c.g, b - p_c.b, a - p_c.a}; }

    constexpr Color operator*(real_t p_s) const { return {r * p_s, g * p_s, b * p_s, a * p_s}; }

    Color& operator+=(const Color& p_c)
    {
        r += p_c.r;
        g += p_c.g;
        b += p_c.b;
        a += p_c.a;
        return *this;
    }

    Color& operator*=(real_t p_s)
    {
        r *= p_s;
        g *= p_s;
        b *= p_s;
        a *= p_s;
        return *this;
    }

    // -- Comparison ----------------------------------------------------------

    constexpr bool operator==(const Color& p_c) const { return r == p_c.r && g == p_c.g && b == p_c.b && a == p_c.a; }

    constexpr bool operator!=(const Color& p_c) const { return !(*this == p_c); }

    // -- Access --------------------------------------------------------------

    real_t& operator[](int p_idx)
    {
        switch (p_idx)
        {
            case 0:
                return r;
            case 1:
                return g;
            case 2:
                return b;
            default:
                return a;
        }
    }

    const real_t& operator[](int p_idx) const
    {
        switch (p_idx)
        {
            case 0:
                return r;
            case 1:
                return g;
            case 2:
                return b;
            default:
                return a;
        }
    }

    // -- Operations ----------------------------------------------------------

    [[nodiscard]] Color lerp(const Color& p_to, real_t p_t) const
    {
        return {
            math_lerp(r, p_to.r, p_t),
            math_lerp(g, p_to.g, p_t),
            math_lerp(b, p_to.b, p_t),
            math_lerp(a, p_to.a, p_t),
        };
    }

    [[nodiscard]] Color clamped() const
    {
        return {
            CLAMP(r, static_cast<real_t>(0), static_cast<real_t>(1)),
            CLAMP(g, static_cast<real_t>(0), static_cast<real_t>(1)),
            CLAMP(b, static_cast<real_t>(0), static_cast<real_t>(1)),
            CLAMP(a, static_cast<real_t>(0), static_cast<real_t>(1)),
        };
    }

    [[nodiscard]] real_t luminance() const
    {
        return static_cast<real_t>(0.2126) * r + static_cast<real_t>(0.7152) * g + static_cast<real_t>(0.0722) * b;
    }

    [[nodiscard]] bool is_equal_approx(const Color& p_c) const
    {
        return math_is_equal_approx(r, p_c.r) && math_is_equal_approx(g, p_c.g) && math_is_equal_approx(b, p_c.b) &&
               math_is_equal_approx(a, p_c.a);
    }

    [[nodiscard]] uint32_t to_rgba32() const
    {
        auto    c  = clamped();
        uint8_t rb = static_cast<uint8_t>(c.r * 255);
        uint8_t gb = static_cast<uint8_t>(c.g * 255);
        uint8_t bb = static_cast<uint8_t>(c.b * 255);
        uint8_t ab = static_cast<uint8_t>(c.a * 255);
        return (uint32_t(rb) << 24) | (uint32_t(gb) << 16) | (uint32_t(bb) << 8) | uint32_t(ab);
    }

    static Color from_rgba32(uint32_t p_rgba)
    {
        return {
            static_cast<real_t>((p_rgba >> 24) & 0xFF) / 255,
            static_cast<real_t>((p_rgba >> 16) & 0xFF) / 255,
            static_cast<real_t>((p_rgba >> 8) & 0xFF) / 255,
            static_cast<real_t>(p_rgba & 0xFF) / 255,
        };
    }

    // -- Constants -----------------------------------------------------------

    static constexpr Color WHITE() { return {1, 1, 1, 1}; }

    static constexpr Color BLACK() { return {0, 0, 0, 1}; }

    static constexpr Color RED() { return {1, 0, 0, 1}; }

    static constexpr Color GREEN() { return {0, 1, 0, 1}; }

    static constexpr Color BLUE() { return {0, 0, 1, 1}; }

    static constexpr Color TRANSPARENT() { return {0, 0, 0, 0}; }
};
