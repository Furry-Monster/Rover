#pragma once

#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/typedefs.h"

namespace rover
{

    /**
     * Column-major 4x4 transform (matches Vulkan / column-vector convention).
     * Element (column `col`, row `row`) is accessed via operator()(col, row).
     */
    struct Mat4
    {
        /** columns[col][row] */
        f32 c[4][4]{};

        constexpr Mat4() noexcept { c[0][0] = c[1][1] = c[2][2] = c[3][3] = 1.0f; }

        constexpr Mat4(f32 c0r0,
                       f32 c0r1,
                       f32 c0r2,
                       f32 c0r3,
                       f32 c1r0,
                       f32 c1r1,
                       f32 c1r2,
                       f32 c1r3,
                       f32 c2r0,
                       f32 c2r1,
                       f32 c2r2,
                       f32 c2r3,
                       f32 c3r0,
                       f32 c3r1,
                       f32 c3r2,
                       f32 c3r3) noexcept
            : c{{c0r0, c0r1, c0r2, c0r3}, {c1r0, c1r1, c1r2, c1r3}, {c2r0, c2r1, c2r2, c2r3}, {c3r0, c3r1, c3r2, c3r3}}
        {}

        [[nodiscard]] constexpr f32& operator()(i32 col, i32 row) noexcept { return c[col][row]; }

        [[nodiscard]] constexpr f32 operator()(i32 col, i32 row) const noexcept { return c[col][row]; }

        [[nodiscard]] Mat4 operator*(const Mat4& rhs) const noexcept;

        [[nodiscard]] constexpr Vector4 operator*(const Vector4& rhs) const noexcept
        {
            return {
                c[0][0] * rhs[0] + c[1][0] * rhs[1] + c[2][0] * rhs[2] + c[3][0] * rhs[3],
                c[0][1] * rhs[0] + c[1][1] * rhs[1] + c[2][1] * rhs[2] + c[3][1] * rhs[3],
                c[0][2] * rhs[0] + c[1][2] * rhs[1] + c[2][2] * rhs[2] + c[3][2] * rhs[3],
                c[0][3] * rhs[0] + c[1][3] * rhs[1] + c[2][3] * rhs[2] + c[3][3] * rhs[3],
            };
        }

        Mat4& operator*=(const Mat4& rhs) noexcept;

        [[nodiscard]] constexpr bool operator==(const Mat4& rhs) const noexcept
        {
            for (int col = 0; col < 4; ++col)
            {
                for (int row = 0; row < 4; ++row)
                {
                    if (c[col][row] != rhs.c[col][row])
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        [[nodiscard]] constexpr bool operator!=(const Mat4& rhs) const noexcept { return !(*this == rhs); }

        [[nodiscard]] Mat4 inverse() const noexcept;

        [[nodiscard]] Mat4 transpose() const noexcept;

        [[nodiscard]] f32 determinant() const noexcept;

        [[nodiscard]] static constexpr Mat4 identity() noexcept { return {}; }

        [[nodiscard]] static constexpr Mat4 translate(const Vector3& t) noexcept
        {
            Mat4 m{};
            m.c[0][0] = m.c[1][1] = m.c[2][2] = m.c[3][3] = 1.0f;
            m.c[3][0]                                     = t.v.x;
            m.c[3][1]                                     = t.v.y;
            m.c[3][2]                                     = t.v.z;
            return m;
        }

        [[nodiscard]] static constexpr Mat4 scale(const Vector3& s) noexcept
        {
            Mat4 m{};
            m.c[0][0] = s.v.x;
            m.c[1][1] = s.v.y;
            m.c[2][2] = s.v.z;
            m.c[3][3] = 1.0f;
            return m;
        }

        /**
         * Perspective for the engine's canonical clip space (targets the same NDC
         * convention as Vulkan, Metal, D3D12: Z in [0,1], Y flipped vs classic
         * OpenGL so that a normal full-frame viewport with positive height yields
         * the expected image). Shaders and `GraphicsDevice` implementations for
         * those APIs should use this matrix as-is. A legacy OpenGL-style backend
         * should apply a small clip correction (e.g. extra uniform, or
         * `glClipControl` + matching viewport) rather than forking these factories
         * per API inside core/math.
         */
        [[nodiscard]] static Mat4 perspective(f32 fovy, f32 aspect, f32 z_near, f32 z_far) noexcept;

        /**
         * Orthographic projection; depth mapping matches `perspective` (canonical
         * / modern-API Z in [0, 1]). Y axis matches `perspective` only if used with
         * the same viewport convention as the active backend.
         */
        [[nodiscard]] static Mat4 ortho(f32 left, f32 right, f32 bottom, f32 top, f32 z_near, f32 z_far) noexcept;

        [[nodiscard]] static Mat4 look_at(const Vector3& eye, const Vector3& center, const Vector3& up) noexcept;

        [[nodiscard]] static Mat4 rotate(f32 angle, const Vector3& axis) noexcept;
    };

    inline Mat4 Mat4::operator*(const Mat4& rhs) const noexcept
    {
        Mat4 out{};
        for (int col = 0; col < 4; ++col)
        {
            const Vector4 v{rhs.c[col][0], rhs.c[col][1], rhs.c[col][2], rhs.c[col][3]};
            const Vector4 o = (*this) * v;
            out.c[col][0]   = o[0];
            out.c[col][1]   = o[1];
            out.c[col][2]   = o[2];
            out.c[col][3]   = o[3];
        }
        return out;
    }

    inline Mat4& Mat4::operator*=(const Mat4& rhs) noexcept
    {
        *this = *this * rhs;
        return *this;
    }

} // namespace rover
