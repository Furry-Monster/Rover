#include "core/math/mat4.h"

#include <cmath>

namespace rover
{

    namespace
    {

        [[nodiscard]] bool invert_gauss_jordan(const Mat4& in, Mat4& out) noexcept
        {
            double a[4][8]{};
            for (int row = 0; row < 4; ++row)
            {
                for (int col = 0; col < 4; ++col)
                {
                    a[row][col] = static_cast<double>(in.c[col][row]);
                }
                for (int col = 0; col < 4; ++col)
                {
                    a[row][col + 4] = (row == col) ? 1.0 : 0.0;
                }
            }

            // Forward elimination with partial pivoting
            for (int col = 0; col < 4; ++col)
            {
                int    pivot = col;
                double best  = std::abs(a[col][col]);
                for (int r = col + 1; r < 4; ++r)
                {
                    const double v = std::abs(a[r][col]);
                    if (v > best)
                    {
                        best  = v;
                        pivot = r;
                    }
                }
                if (best < 1e-12)
                {
                    return false;
                }
                if (pivot != col)
                {
                    for (int k = 0; k < 8; ++k)
                    {
                        std::swap(a[col][k], a[pivot][k]);
                    }
                }

                const double inv_diag = 1.0 / a[col][col];
                for (int k = 0; k < 8; ++k)
                {
                    a[col][k] *= inv_diag;
                }

                for (int r = 0; r < 4; ++r)
                {
                    if (r == col)
                    {
                        continue;
                    }
                    const double factor = a[r][col];
                    if (factor == 0.0)
                    {
                        continue;
                    }
                    for (int k = 0; k < 8; ++k)
                    {
                        a[r][k] -= factor * a[col][k];
                    }
                }
            }

            for (int row = 0; row < 4; ++row)
            {
                for (int col = 0; col < 4; ++col)
                {
                    out.c[col][row] = static_cast<f32>(a[row][col + 4]);
                }
            }
            return true;
        }

    } // namespace

    Mat4 Mat4::transpose() const noexcept
    {
        Mat4 t{};
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
            {
                t.c[row][col] = c[col][row];
            }
        }
        return t;
    }

    f32 Mat4::determinant() const noexcept
    {
        const Mat4& m = *this;
        // Laplace expansion along column 0; operator()(col,row) indexes columns[col][row].
        auto det3 = [](const Mat4& M, i32 r0, i32 r1, i32 r2, i32 dc1, i32 dc2, i32 dc3) noexcept -> f32 {
            return M(dc1, r0) * (M(dc2, r1) * M(dc3, r2) - M(dc3, r1) * M(dc2, r2)) -
                   M(dc2, r0) * (M(dc1, r1) * M(dc3, r2) - M(dc3, r1) * M(dc1, r2)) +
                   M(dc3, r0) * (M(dc1, r1) * M(dc2, r2) - M(dc2, r1) * M(dc1, r2));
        };

        return m(0, 0) * det3(m, 1, 2, 3, 1, 2, 3) - m(0, 1) * det3(m, 0, 2, 3, 1, 2, 3) +
               m(0, 2) * det3(m, 0, 1, 3, 1, 2, 3) - m(0, 3) * det3(m, 0, 1, 2, 1, 2, 3);
    }

    Mat4 Mat4::inverse() const noexcept
    {
        Mat4 inv{};
        if (!invert_gauss_jordan(*this, inv))
        {
            return {};
        }
        return inv;
    }

    Mat4 Mat4::perspective(f32 fovy, f32 aspect, f32 z_near, f32 z_far) noexcept
    {
        const f32 tan_half = std::tan(fovy * 0.5f);
        Mat4      out{};
        out.c[0][0] = 1.0f / (aspect * tan_half);
        out.c[1][1] = 1.0f / tan_half;
        out.c[2][2] = z_far / (z_near - z_far);
        out.c[2][3] = -1.0f;
        out.c[3][2] = (z_far * z_near) / (z_near - z_far);
        out.c[3][3] = 0.0f;
        return out;
    }

    Mat4 Mat4::ortho(f32 left, f32 right, f32 bottom, f32 top, f32 z_near, f32 z_far) noexcept
    {
        Mat4      out{};
        const f32 rl = right - left;
        const f32 tb = top - bottom;
        const f32 fn = z_far - z_near;

        out.c[0][0] = 2.0f / rl;
        out.c[1][1] = 2.0f / tb;
        out.c[2][2] = -1.0f / fn;
        out.c[3][0] = -(right + left) / rl;
        out.c[3][1] = -(top + bottom) / tb;
        out.c[3][2] = -z_near / fn;
        out.c[3][3] = 1.0f;
        return out;
    }

    Mat4 Mat4::look_at(const Vector3& eye, const Vector3& center, const Vector3& up) noexcept
    {
        const Vector3 f = (center - eye).normalized();
        Vector3       s = f.cross(up).normalized();
        Vector3       u = s.cross(f);

        Mat4 out{};
        out.c[0][0] = s.v.x;
        out.c[0][1] = s.v.y;
        out.c[0][2] = s.v.z;
        out.c[0][3] = 0.0f;

        out.c[1][0] = u.v.x;
        out.c[1][1] = u.v.y;
        out.c[1][2] = u.v.z;
        out.c[1][3] = 0.0f;

        out.c[2][0] = -f.v.x;
        out.c[2][1] = -f.v.y;
        out.c[2][2] = -f.v.z;
        out.c[2][3] = 0.0f;

        out.c[3][0] = -s.dot(eye);
        out.c[3][1] = -u.dot(eye);
        out.c[3][2] = f.dot(eye);
        out.c[3][3] = 1.0f;

        return out;
    }

    Mat4 Mat4::rotate(f32 angle, const Vector3& axis) noexcept
    {
        const Vector3 n = axis.normalized();
        const f32     x = n.v.x;
        const f32     y = n.v.y;
        const f32     z = n.v.z;
        const f32     c = std::cos(angle);
        const f32     s = std::sin(angle);
        const f32     t = 1.0f - c;

        Mat4 out{};
        out.c[0][0] = t * x * x + c;
        out.c[1][0] = t * x * y + s * z;
        out.c[2][0] = t * x * z - s * y;
        out.c[3][0] = 0.0f;

        out.c[0][1] = t * x * y - s * z;
        out.c[1][1] = t * y * y + c;
        out.c[2][1] = t * y * z + s * x;
        out.c[3][1] = 0.0f;

        out.c[0][2] = t * x * z + s * y;
        out.c[1][2] = t * y * z - s * x;
        out.c[2][2] = t * z * z + c;
        out.c[3][2] = 0.0f;

        out.c[0][3] = 0.0f;
        out.c[1][3] = 0.0f;
        out.c[2][3] = 0.0f;
        out.c[3][3] = 1.0f;

        return out;
    }

} // namespace rover
