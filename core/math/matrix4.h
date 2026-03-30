#pragma once

#include "core/math/vector3.h"
#include "core/math/vector4.h"

/**
 * @brief
 *
 * 4x4 column-major matrix, used primarily for projection and view transforms.
 * Data layout: columns[col][row] — compatible with OpenGL/Vulkan conventions.
 */
struct Matrix4
{
    Vector4 columns[4] = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    };

    constexpr Matrix4() = default;

    constexpr Matrix4(const Vector4& p_c0, const Vector4& p_c1, const Vector4& p_c2, const Vector4& p_c3)
        : columns{p_c0, p_c1, p_c2, p_c3}
    {}

    // -- Access --------------------------------------------------------------

    Vector4& operator[](int p_col) { return columns[p_col]; }

    const Vector4& operator[](int p_col) const { return columns[p_col]; }

    // -- Arithmetic ----------------------------------------------------------

    Matrix4 operator*(const Matrix4& p_m) const
    {
        Matrix4 result;
        for (int c = 0; c < 4; ++c)
        {
            for (int r = 0; r < 4; ++r)
            {
                result.columns[c][r] = columns[0][r] * p_m.columns[c][0] + columns[1][r] * p_m.columns[c][1] +
                                       columns[2][r] * p_m.columns[c][2] + columns[3][r] * p_m.columns[c][3];
            }
        }
        return result;
    }

    Vector4 operator*(const Vector4& p_v) const
    {
        return {
            columns[0][0] * p_v.x + columns[1][0] * p_v.y + columns[2][0] * p_v.z + columns[3][0] * p_v.w,
            columns[0][1] * p_v.x + columns[1][1] * p_v.y + columns[2][1] * p_v.z + columns[3][1] * p_v.w,
            columns[0][2] * p_v.x + columns[1][2] * p_v.y + columns[2][2] * p_v.z + columns[3][2] * p_v.w,
            columns[0][3] * p_v.x + columns[1][3] * p_v.y + columns[2][3] * p_v.z + columns[3][3] * p_v.w,
        };
    }

    bool operator==(const Matrix4& p_m) const
    {
        return columns[0] == p_m.columns[0] && columns[1] == p_m.columns[1] && columns[2] == p_m.columns[2] &&
               columns[3] == p_m.columns[3];
    }

    bool operator!=(const Matrix4& p_m) const { return !(*this == p_m); }

    // -- Operations ----------------------------------------------------------

    [[nodiscard]] Matrix4 transposed() const
    {
        Matrix4 r;
        for (int c = 0; c < 4; ++c)
        {
            for (int row = 0; row < 4; ++row)
            {
                r.columns[c][row] = columns[row][c];
            }
        }
        return r;
    }

    // -- Factory methods (Vulkan conventions: RH, Y-up, depth [0,1]) ---------

    [[nodiscard]] static Matrix4 perspective(real_t p_fov_y_rad, real_t p_aspect, real_t p_near, real_t p_far)
    {
        real_t  tan_half = std::tan(p_fov_y_rad / 2);
        Matrix4 r;
        r.columns[0] = {1 / (p_aspect * tan_half), 0, 0, 0};
        r.columns[1] = {0, -1 / tan_half, 0, 0};
        r.columns[2] = {0, 0, p_far / (p_near - p_far), -1};
        r.columns[3] = {0, 0, (p_near * p_far) / (p_near - p_far), 0};
        return r;
    }

    [[nodiscard]] static Matrix4 orthographic(real_t p_left,
                                              real_t p_right,
                                              real_t p_bottom,
                                              real_t p_top,
                                              real_t p_near,
                                              real_t p_far)
    {
        Matrix4 r;
        r.columns[0] = {2 / (p_right - p_left), 0, 0, 0};
        r.columns[1] = {0, 2 / (p_top - p_bottom), 0, 0};
        r.columns[2] = {0, 0, 1 / (p_near - p_far), 0};
        r.columns[3] = {
            -(p_right + p_left) / (p_right - p_left),
            -(p_top + p_bottom) / (p_top - p_bottom),
            p_near / (p_near - p_far),
            1,
        };
        return r;
    }

    [[nodiscard]] static Matrix4 look_at(const Vector3& p_eye,
                                         const Vector3& p_target,
                                         const Vector3& p_up = Vector3::UP())
    {
        Vector3 f = (p_target - p_eye).normalized();
        Vector3 r = f.cross(p_up).normalized();
        Vector3 u = r.cross(f);

        Matrix4 m;
        m.columns[0] = {r.x, u.x, -f.x, 0};
        m.columns[1] = {r.y, u.y, -f.y, 0};
        m.columns[2] = {r.z, u.z, -f.z, 0};
        m.columns[3] = {-r.dot(p_eye), -u.dot(p_eye), f.dot(p_eye), 1};
        return m;
    }

    // -- Constants -----------------------------------------------------------

    static constexpr Matrix4 IDENTITY() { return {}; }
};
