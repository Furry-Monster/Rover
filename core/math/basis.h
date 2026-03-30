#pragma once

#include "core/math/quaternion.h"
#include "core/math/vector3.h"

/**
 * @brief
 *
 * 3x3 matrix representing rotation and scale.
 * Stored as three row vectors (row-major).
 */
struct Basis
{
    Vector3 rows[3] = {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1},
    };

    constexpr Basis() = default;

    constexpr Basis(const Vector3& p_row0, const Vector3& p_row1, const Vector3& p_row2) : rows{p_row0, p_row1, p_row2}
    {}

    explicit Basis(const Quaternion& p_q)
    {
        real_t d = p_q.length_squared();
        real_t s = static_cast<real_t>(2.0) / d;

        real_t xs = p_q.x * s, ys = p_q.y * s, zs = p_q.z * s;
        real_t wx = p_q.w * xs, wy = p_q.w * ys, wz = p_q.w * zs;
        real_t xx = p_q.x * xs, xy = p_q.x * ys, xz = p_q.x * zs;
        real_t yy = p_q.y * ys, yz = p_q.y * zs, zz = p_q.z * zs;

        rows[0] = {1 - (yy + zz), xy - wz, xz + wy};
        rows[1] = {xy + wz, 1 - (xx + zz), yz - wx};
        rows[2] = {xz - wy, yz + wx, 1 - (xx + yy)};
    }

    // -- Access --------------------------------------------------------------

    Vector3& operator[](int p_row) { return rows[p_row]; }

    const Vector3& operator[](int p_row) const { return rows[p_row]; }

    [[nodiscard]] Vector3 get_column(int p_col) const { return {rows[0][p_col], rows[1][p_col], rows[2][p_col]}; }

    void set_column(int p_col, const Vector3& p_v)
    {
        rows[0][p_col] = p_v.x;
        rows[1][p_col] = p_v.y;
        rows[2][p_col] = p_v.z;
    }

    // -- Arithmetic ----------------------------------------------------------

    Vector3 operator*(const Vector3& p_v) const
    {
        return {
            rows[0].dot(p_v),
            rows[1].dot(p_v),
            rows[2].dot(p_v),
        };
    }

    Basis operator*(const Basis& p_b) const
    {
        return {
            {rows[0].dot(p_b.get_column(0)), rows[0].dot(p_b.get_column(1)), rows[0].dot(p_b.get_column(2))},
            {rows[1].dot(p_b.get_column(0)), rows[1].dot(p_b.get_column(1)), rows[1].dot(p_b.get_column(2))},
            {rows[2].dot(p_b.get_column(0)), rows[2].dot(p_b.get_column(1)), rows[2].dot(p_b.get_column(2))},
        };
    }

    constexpr bool operator==(const Basis& p_b) const
    {
        return rows[0] == p_b.rows[0] && rows[1] == p_b.rows[1] && rows[2] == p_b.rows[2];
    }

    constexpr bool operator!=(const Basis& p_b) const { return !(*this == p_b); }

    // -- Operations ----------------------------------------------------------

    [[nodiscard]] Basis transposed() const
    {
        return {
            {rows[0].x, rows[1].x, rows[2].x},
            {rows[0].y, rows[1].y, rows[2].y},
            {rows[0].z, rows[1].z, rows[2].z},
        };
    }

    [[nodiscard]] real_t determinant() const
    {
        return rows[0].x * (rows[1].y * rows[2].z - rows[1].z * rows[2].y) -
               rows[0].y * (rows[1].x * rows[2].z - rows[1].z * rows[2].x) +
               rows[0].z * (rows[1].x * rows[2].y - rows[1].y * rows[2].x);
    }

    [[nodiscard]] Basis inverse() const
    {
        real_t det = determinant();
        if (math_is_zero_approx(det))
        {
            return {};
        }
        real_t inv_det = static_cast<real_t>(1) / det;

        return {
            {(rows[1].y * rows[2].z - rows[1].z * rows[2].y) * inv_det,
             (rows[0].z * rows[2].y - rows[0].y * rows[2].z) * inv_det,
             (rows[0].y * rows[1].z - rows[0].z * rows[1].y) * inv_det},
            {(rows[1].z * rows[2].x - rows[1].x * rows[2].z) * inv_det,
             (rows[0].x * rows[2].z - rows[0].z * rows[2].x) * inv_det,
             (rows[0].z * rows[1].x - rows[0].x * rows[1].z) * inv_det},
            {(rows[1].x * rows[2].y - rows[1].y * rows[2].x) * inv_det,
             (rows[0].y * rows[2].x - rows[0].x * rows[2].y) * inv_det,
             (rows[0].x * rows[1].y - rows[0].y * rows[1].x) * inv_det},
        };
    }

    [[nodiscard]] Quaternion get_quaternion() const
    {
        real_t     trace = rows[0].x + rows[1].y + rows[2].z;
        Quaternion q;

        if (trace > 0)
        {
            real_t s = std::sqrt(trace + 1) * 2;
            q.w      = static_cast<real_t>(0.25) * s;
            q.x      = (rows[2].y - rows[1].z) / s;
            q.y      = (rows[0].z - rows[2].x) / s;
            q.z      = (rows[1].x - rows[0].y) / s;
        }
        else if (rows[0].x > rows[1].y && rows[0].x > rows[2].z)
        {
            real_t s = std::sqrt(1 + rows[0].x - rows[1].y - rows[2].z) * 2;
            q.w      = (rows[2].y - rows[1].z) / s;
            q.x      = static_cast<real_t>(0.25) * s;
            q.y      = (rows[0].y + rows[1].x) / s;
            q.z      = (rows[0].z + rows[2].x) / s;
        }
        else if (rows[1].y > rows[2].z)
        {
            real_t s = std::sqrt(1 + rows[1].y - rows[0].x - rows[2].z) * 2;
            q.w      = (rows[0].z - rows[2].x) / s;
            q.x      = (rows[0].y + rows[1].x) / s;
            q.y      = static_cast<real_t>(0.25) * s;
            q.z      = (rows[1].z + rows[2].y) / s;
        }
        else
        {
            real_t s = std::sqrt(1 + rows[2].z - rows[0].x - rows[1].y) * 2;
            q.w      = (rows[1].x - rows[0].y) / s;
            q.x      = (rows[0].z + rows[2].x) / s;
            q.y      = (rows[1].z + rows[2].y) / s;
            q.z      = static_cast<real_t>(0.25) * s;
        }

        return q;
    }

    [[nodiscard]] Vector3 get_scale() const
    {
        return {
            get_column(0).length(),
            get_column(1).length(),
            get_column(2).length(),
        };
    }

    // -- Constants -----------------------------------------------------------

    static Basis IDENTITY() { return {}; }
};
