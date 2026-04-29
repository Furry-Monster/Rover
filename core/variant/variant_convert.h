#pragma once

#include "core/variant/variant.h"

#include <string>
#include <type_traits>

// ---------------------------------------------------------------------------
// Conversions between native C++ types and Variant.
//
// Used by reflection helpers (e.g. ROVER_BIND_PROPERTY) and by the
// serialization module. Only narrow, lossless conversions are provided here;
// callers that need fancier coercions should use Variant accessors directly.
// ---------------------------------------------------------------------------

namespace rover
{
    namespace detail
    {

        template <typename T>
        inline T variant_to(const Variant& v);

        template <>
        inline bool variant_to<bool>(const Variant& v)
        {
            return v.as_bool();
        }

        template <>
        inline i32 variant_to<i32>(const Variant& v)
        {
            return static_cast<i32>(v.as_int());
        }

        template <>
        inline i64 variant_to<i64>(const Variant& v)
        {
            return v.as_int();
        }

        template <>
        inline u32 variant_to<u32>(const Variant& v)
        {
            return static_cast<u32>(v.as_int());
        }

        template <>
        inline u64 variant_to<u64>(const Variant& v)
        {
            return static_cast<u64>(v.as_int());
        }

        template <>
        inline f32 variant_to<f32>(const Variant& v)
        {
            return static_cast<f32>(v.as_float());
        }

        template <>
        inline f64 variant_to<f64>(const Variant& v)
        {
            return v.as_float();
        }

        template <>
        inline std::string variant_to<std::string>(const Variant& v)
        {
            return v.as_string();
        }

        template <>
        inline Vector2 variant_to<Vector2>(const Variant& v)
        {
            return v.as_vector2();
        }

        template <>
        inline Vector3 variant_to<Vector3>(const Variant& v)
        {
            return v.as_vector3();
        }

        template <>
        inline Vector4 variant_to<Vector4>(const Variant& v)
        {
            return v.as_vector4();
        }

        template <>
        inline Quat variant_to<Quat>(const Variant& v)
        {
            return v.as_quat();
        }

        template <>
        inline Mat4 variant_to<Mat4>(const Variant& v)
        {
            return v.as_mat4();
        }

    } // namespace detail
} // namespace rover
