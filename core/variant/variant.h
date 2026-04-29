#pragma once

#include "core/math/mat4.h"
#include "core/math/quat.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/typedefs.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rover
{

    class Object;
    class Variant;

    using VariantArray = std::vector<Variant>;
    using VariantDict  = std::unordered_map<std::string, Variant>;

    // ---------------------------------------------------------------------------
    // Variant: tagged union over engine-friendly types.
    //
    // Implementation strategy (ADR-0009): manual union+tag (Godot-inspired).
    // Inline POD types up to 64 bytes (Mat4 fits exactly). Heap-allocated strings,
    // arrays, and dictionaries via shared pointers so copies are cheap.
    // ---------------------------------------------------------------------------
    class Variant
    {
    public:
        enum class Type : u8
        {
            Nil = 0,
            Bool,
            Int,
            Float,
            String,
            Vector2,
            Vector3,
            Vector4,
            Quat,
            Mat4,
            Color,
            ObjectPtr,
            Array,
            Dictionary,
        };

        // ---- Construction ----
        Variant() noexcept = default;

        ~Variant() { clear(); }

        Variant(const Variant& other);
        Variant(Variant&& other) noexcept;
        Variant& operator=(const Variant& other);
        Variant& operator=(Variant&& other) noexcept;

        Variant(bool v);
        Variant(i32 v);
        Variant(i64 v);
        Variant(u32 v);
        Variant(f32 v);
        Variant(f64 v);
        Variant(const char* v);
        Variant(std::string v);
        Variant(const Vector2& v);
        Variant(const Vector3& v);
        Variant(const Vector4& v);
        Variant(const Quat& v);
        Variant(const Mat4& v);
        Variant(Object* v);
        Variant(VariantArray v);
        Variant(VariantDict v);

        // ---- Type queries ----
        [[nodiscard]] Type type() const noexcept { return type_; }

        [[nodiscard]] bool is_nil() const noexcept { return type_ == Type::Nil; }

        [[nodiscard]] bool is_num() const noexcept
        {
            return type_ == Type::Int || type_ == Type::Float || type_ == Type::Bool;
        }

        [[nodiscard]] const char* type_name() const noexcept;

        // ---- Typed access (no conversion) ----
        [[nodiscard]] bool                as_bool() const noexcept;
        [[nodiscard]] i64                 as_int() const noexcept;
        [[nodiscard]] f64                 as_float() const noexcept;
        [[nodiscard]] const std::string&  as_string() const;
        [[nodiscard]] const Vector2&      as_vector2() const noexcept;
        [[nodiscard]] const Vector3&      as_vector3() const noexcept;
        [[nodiscard]] const Vector4&      as_vector4() const noexcept;
        [[nodiscard]] const Quat&         as_quat() const noexcept;
        [[nodiscard]] const Mat4&         as_mat4() const noexcept;
        [[nodiscard]] Object*             as_object() const noexcept;
        [[nodiscard]] const VariantArray& as_array() const;
        [[nodiscard]] const VariantDict&  as_dict() const;

        // Mutating accessors for arrays / dicts
        [[nodiscard]] VariantArray& array_mut();
        [[nodiscard]] VariantDict&  dict_mut();

        // ---- Comparison ----
        [[nodiscard]] bool operator==(const Variant& rhs) const noexcept;

        [[nodiscard]] bool operator!=(const Variant& rhs) const noexcept { return !(*this == rhs); }

        // ---- Hash (for use as Dictionary key candidates in future) ----
        [[nodiscard]] usize hash() const noexcept;

        // ---- Reset to Nil and free heap data ----
        void clear() noexcept;

    private:
        Type type_ = Type::Nil;

        // 64-byte storage allows Mat4 (64B) inline.
        // Heap types use shared_ptr stored in storage_.
        union Storage
        {
            bool    b;
            i64     i;
            f64     f;
            Vector2 v2;
            Vector3 v3;
            Vector4 v4;
            Quat    q;
            Mat4    m4;
            Object* obj;

            // Heap-backed types use shared_ptr placed inside the union.
            std::shared_ptr<std::string>  str;
            std::shared_ptr<VariantArray> arr;
            std::shared_ptr<VariantDict>  dict;

            Storage() {}

            ~Storage() {}
        } storage_{};

        void copy_from(const Variant& other);
        void move_from(Variant&& other) noexcept;
    };

    static_assert(sizeof(Variant) <= 80, "Variant should fit in two cache lines");

} // namespace rover
