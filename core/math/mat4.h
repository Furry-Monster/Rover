#pragma once

#include "core/typedefs.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rover {

struct Mat4 {
    glm::mat4 m{1.0f};

    constexpr Mat4() noexcept = default;
    constexpr Mat4(glm::mat4 mat) noexcept : m(mat) {}
    constexpr Mat4(f32 c0r0, f32 c0r1, f32 c0r2, f32 c0r3,
                   f32 c1r0, f32 c1r1, f32 c1r2, f32 c1r3,
                   f32 c2r0, f32 c2r1, f32 c2r2, f32 c2r3,
                   f32 c3r0, f32 c3r1, f32 c3r2, f32 c3r3) noexcept
        : m(c0r0, c0r1, c0r2, c0r3,
            c1r0, c1r1, c1r2, c1r3,
            c2r0, c2r1, c2r2, c2r3,
            c3r0, c3r1, c3r2, c3r3) {}

    [[nodiscard]] constexpr operator glm::mat4() const noexcept { return m; }

    // Element access (column, row)
    [[nodiscard]] constexpr f32& operator()(i32 col, i32 row) noexcept { return m[col][row]; }
    [[nodiscard]] constexpr f32  operator()(i32 col, i32 row) const noexcept { return m[col][row]; }

    // Arithmetic
    [[nodiscard]] constexpr Mat4 operator*(const Mat4& rhs) const noexcept { return {m * rhs.m}; }
    [[nodiscard]] constexpr Vector4 operator*(const Vector4& rhs) const noexcept { return {m * rhs.v}; }
    Mat4& operator*=(const Mat4& rhs) noexcept { m *= rhs.m; return *this; }

    [[nodiscard]] constexpr bool operator==(const Mat4& rhs) const noexcept { return m == rhs.m; }
    [[nodiscard]] constexpr bool operator!=(const Mat4& rhs) const noexcept { return m != rhs.m; }

    // Operations
    [[nodiscard]] Mat4 inverse() const noexcept { return {glm::inverse(m)}; }
    [[nodiscard]] Mat4 transpose() const noexcept { return {glm::transpose(m)}; }
    [[nodiscard]] f32 determinant() const noexcept { return glm::determinant(m); }

    // Factory methods
    [[nodiscard]] static constexpr Mat4 identity() noexcept { return {}; }

    [[nodiscard]] static Mat4 translate(const Vector3& translation) noexcept {
        return {glm::translate(glm::mat4{1.0f}, translation.v)};
    }

    [[nodiscard]] static Mat4 rotate(f32 angle, const Vector3& axis) noexcept {
        return {glm::rotate(glm::mat4{1.0f}, angle, axis.v)};
    }

    [[nodiscard]] static Mat4 scale(const Vector3& s) noexcept {
        return {glm::scale(glm::mat4{1.0f}, s.v)};
    }

    [[nodiscard]] static Mat4 perspective(f32 fov, f32 aspect, f32 near, f32 far) noexcept {
        return {glm::perspective(fov, aspect, near, far)};
    }

    [[nodiscard]] static Mat4 ortho(f32 left, f32 right, f32 bottom, f32 top,
                                    f32 near, f32 far) noexcept {
        return {glm::ortho(left, right, bottom, top, near, far)};
    }

    [[nodiscard]] static Mat4 look_at(const Vector3& eye, const Vector3& center,
                                      const Vector3& up) noexcept {
        return {glm::lookAt(eye.v, center.v, up.v)};
    }
};

} // namespace rover
