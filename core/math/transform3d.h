#pragma once

#include "core/math/basis.h"

struct Transform3D
{
    Basis   basis;
    Vector3 origin;

    Transform3D() = default;

    Transform3D(const Basis& p_basis, const Vector3& p_origin) : basis(p_basis), origin(p_origin) {}

    Transform3D(const Quaternion& p_quat, const Vector3& p_origin) : basis(p_quat), origin(p_origin) {}

    // -- Transform operations ------------------------------------------------

    [[nodiscard]] Vector3 xform(const Vector3& p_v) const { return basis * p_v + origin; }

    [[nodiscard]] Vector3 xform_inv(const Vector3& p_v) const
    {
        Vector3 v = p_v - origin;
        return basis.transposed() * v;
    }

    Transform3D operator*(const Transform3D& p_t) const { return {basis * p_t.basis, xform(p_t.origin)}; }

    bool operator==(const Transform3D& p_t) const { return basis == p_t.basis && origin == p_t.origin; }

    bool operator!=(const Transform3D& p_t) const { return !(*this == p_t); }

    // -- Operations ----------------------------------------------------------

    [[nodiscard]] Transform3D inverse() const
    {
        Basis b_inv = basis.inverse();
        return {b_inv, b_inv * (-origin)};
    }

    [[nodiscard]] Transform3D affine_inverse() const
    {
        Basis b_inv = basis.transposed();
        return {b_inv, b_inv * (-origin)};
    }

    void translate(const Vector3& p_offset) { origin += basis * p_offset; }

    void set_look_at(const Vector3& p_eye, const Vector3& p_target, const Vector3& p_up = Vector3::UP())
    {
        Vector3 forward = (p_target - p_eye).normalized();
        Vector3 right   = p_up.cross(forward).normalized();
        Vector3 up      = forward.cross(right);

        basis[0] = right;
        basis[1] = up;
        basis[2] = forward;
        origin   = p_eye;
    }

    // -- Constants -----------------------------------------------------------

    static Transform3D IDENTITY() { return {}; }
};
