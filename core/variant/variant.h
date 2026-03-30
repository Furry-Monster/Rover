#pragma once

#include "core/math/color.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/string/string_name.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class Object;

class Variant
{
public:
    enum Type
    {
        NIL,
        BOOL,
        INT,
        FLOAT,
        STRING,
        VECTOR2,
        VECTOR3,
        VECTOR4,
        COLOR,
        OBJECT,
        ARRAY,
        DICTIONARY,
        TYPE_MAX,
    };

    // -- Constructors --------------------------------------------------------

    Variant() = default;

    Variant(bool p_val);
    Variant(int32_t p_val);
    Variant(int64_t p_val);
    Variant(float p_val);
    Variant(double p_val);
    Variant(const char* p_val);
    Variant(const std::string& p_val);
    Variant(const StringName& p_val);
    Variant(const Vector2& p_val);
    Variant(const Vector3& p_val);
    Variant(const Vector4& p_val);
    Variant(const Color& p_val);
    Variant(Object* p_val);

    Variant(const Variant& p_other);
    Variant(Variant&& p_other) noexcept;
    ~Variant();

    Variant& operator=(const Variant& p_other);
    Variant& operator=(Variant&& p_other) noexcept;

    // -- Type queries --------------------------------------------------------

    [[nodiscard]] Type get_type() const { return _type; }

    [[nodiscard]] bool is_nil() const { return _type == NIL; }

    [[nodiscard]] bool is_num() const { return _type == INT || _type == FLOAT; }

    [[nodiscard]] static const char* get_type_name(Type p_type);

    explicit operator bool() const;

    // -- Getters (with implicit conversion) ----------------------------------

    [[nodiscard]] bool        as_bool() const;
    [[nodiscard]] int64_t     as_int() const;
    [[nodiscard]] double      as_float() const;
    [[nodiscard]] std::string as_string() const;
    [[nodiscard]] Vector2     as_vector2() const;
    [[nodiscard]] Vector3     as_vector3() const;
    [[nodiscard]] Vector4     as_vector4() const;
    [[nodiscard]] Color       as_color() const;
    [[nodiscard]] Object*     as_object() const;

    // -- Comparison ----------------------------------------------------------

    bool operator==(const Variant& p_other) const;
    bool operator!=(const Variant& p_other) const;

    // -- Hash ----------------------------------------------------------------

    [[nodiscard]] uint32_t hash() const;

private:
    Type _type = NIL;

    // clang-format off
    union {
        bool     _bool;
        int64_t  _int;
        double   _float;
        Object*  _obj;
        void*    _ptr;
        uint8_t  _mem[sizeof(real_t) * 4];
    } _data{};

    // clang-format on

    void _clear();
    void _copy_from(const Variant& p_other);
};

// Container types (defined after Variant is complete so they can reference it).
using VariantArray      = std::vector<Variant>;
using VariantDictionary = std::vector<std::pair<StringName, Variant>>;
