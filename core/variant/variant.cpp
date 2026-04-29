#include "core/variant/variant.h"

#include <cstring>
#include <functional>
#include <utility>

namespace rover
{

    // ---------------------------------------------------------------------------
    // Construction helpers
    // ---------------------------------------------------------------------------

    Variant::Variant(bool v) : type_(Type::Bool)
    {
        storage_.b = v;
    }

    Variant::Variant(i32 v) : type_(Type::Int)
    {
        storage_.i = static_cast<i64>(v);
    }

    Variant::Variant(i64 v) : type_(Type::Int)
    {
        storage_.i = v;
    }

    Variant::Variant(u32 v) : type_(Type::Int)
    {
        storage_.i = static_cast<i64>(v);
    }

    Variant::Variant(f32 v) : type_(Type::Float)
    {
        storage_.f = static_cast<f64>(v);
    }

    Variant::Variant(f64 v) : type_(Type::Float)
    {
        storage_.f = v;
    }

    Variant::Variant(const char* v) : type_(Type::String)
    {
        new (&storage_.str) std::shared_ptr<std::string>(std::make_shared<std::string>(v ? v : ""));
    }

    Variant::Variant(std::string v) : type_(Type::String)
    {
        new (&storage_.str) std::shared_ptr<std::string>(std::make_shared<std::string>(std::move(v)));
    }

    Variant::Variant(const Vector2& v) : type_(Type::Vector2)
    {
        storage_.v2 = v;
    }

    Variant::Variant(const Vector3& v) : type_(Type::Vector3)
    {
        storage_.v3 = v;
    }

    Variant::Variant(const Vector4& v) : type_(Type::Vector4)
    {
        storage_.v4 = v;
    }

    Variant::Variant(const Quat& v) : type_(Type::Quat)
    {
        storage_.q = v;
    }

    Variant::Variant(const Mat4& v) : type_(Type::Mat4)
    {
        storage_.m4 = v;
    }

    Variant::Variant(Object* v) : type_(Type::ObjectPtr)
    {
        storage_.obj = v;
    }

    Variant::Variant(VariantArray v) : type_(Type::Array)
    {
        new (&storage_.arr) std::shared_ptr<VariantArray>(std::make_shared<VariantArray>(std::move(v)));
    }

    Variant::Variant(VariantDict v) : type_(Type::Dictionary)
    {
        new (&storage_.dict) std::shared_ptr<VariantDict>(std::make_shared<VariantDict>(std::move(v)));
    }

    // ---------------------------------------------------------------------------
    // Copy / move
    // ---------------------------------------------------------------------------

    Variant::Variant(const Variant& other)
    {
        copy_from(other);
    }

    Variant::Variant(Variant&& other) noexcept
    {
        move_from(std::move(other));
    }

    Variant& Variant::operator=(const Variant& other)
    {
        if (this != &other)
        {
            clear();
            copy_from(other);
        }
        return *this;
    }

    Variant& Variant::operator=(Variant&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            move_from(std::move(other));
        }
        return *this;
    }

    void Variant::copy_from(const Variant& other)
    {
        type_ = other.type_;
        switch (type_)
        {
            case Type::Nil:
                break;
            case Type::Bool:
                storage_.b = other.storage_.b;
                break;
            case Type::Int:
                storage_.i = other.storage_.i;
                break;
            case Type::Float:
                storage_.f = other.storage_.f;
                break;
            case Type::Vector2:
                storage_.v2 = other.storage_.v2;
                break;
            case Type::Vector3:
                storage_.v3 = other.storage_.v3;
                break;
            case Type::Vector4:
            case Type::Color:
                storage_.v4 = other.storage_.v4;
                break;
            case Type::Quat:
                storage_.q = other.storage_.q;
                break;
            case Type::Mat4:
                storage_.m4 = other.storage_.m4;
                break;
            case Type::ObjectPtr:
                storage_.obj = other.storage_.obj;
                break;
            case Type::String:
                new (&storage_.str) std::shared_ptr<std::string>(other.storage_.str);
                break;
            case Type::Array:
                new (&storage_.arr) std::shared_ptr<VariantArray>(other.storage_.arr);
                break;
            case Type::Dictionary:
                new (&storage_.dict) std::shared_ptr<VariantDict>(other.storage_.dict);
                break;
        }
    }

    void Variant::move_from(Variant&& other) noexcept
    {
        type_ = other.type_;
        switch (type_)
        {
            case Type::Nil:
                break;
            case Type::Bool:
                storage_.b = other.storage_.b;
                break;
            case Type::Int:
                storage_.i = other.storage_.i;
                break;
            case Type::Float:
                storage_.f = other.storage_.f;
                break;
            case Type::Vector2:
                storage_.v2 = other.storage_.v2;
                break;
            case Type::Vector3:
                storage_.v3 = other.storage_.v3;
                break;
            case Type::Vector4:
            case Type::Color:
                storage_.v4 = other.storage_.v4;
                break;
            case Type::Quat:
                storage_.q = other.storage_.q;
                break;
            case Type::Mat4:
                storage_.m4 = other.storage_.m4;
                break;
            case Type::ObjectPtr:
                storage_.obj = other.storage_.obj;
                break;
            case Type::String:
                new (&storage_.str) std::shared_ptr<std::string>(std::move(other.storage_.str));
                break;
            case Type::Array:
                new (&storage_.arr) std::shared_ptr<VariantArray>(std::move(other.storage_.arr));
                break;
            case Type::Dictionary:
                new (&storage_.dict) std::shared_ptr<VariantDict>(std::move(other.storage_.dict));
                break;
        }
        other.clear();
    }

    void Variant::clear() noexcept
    {
        switch (type_)
        {
            case Type::String:
                storage_.str.~shared_ptr();
                break;
            case Type::Array:
                storage_.arr.~shared_ptr();
                break;
            case Type::Dictionary:
                storage_.dict.~shared_ptr();
                break;
            default:
                break;
        }
        type_ = Type::Nil;
    }

    // ---------------------------------------------------------------------------
    // Type queries
    // ---------------------------------------------------------------------------

    const char* Variant::type_name() const noexcept
    {
        switch (type_)
        {
            case Type::Nil:
                return "Nil";
            case Type::Bool:
                return "Bool";
            case Type::Int:
                return "Int";
            case Type::Float:
                return "Float";
            case Type::String:
                return "String";
            case Type::Vector2:
                return "Vector2";
            case Type::Vector3:
                return "Vector3";
            case Type::Vector4:
                return "Vector4";
            case Type::Quat:
                return "Quat";
            case Type::Mat4:
                return "Mat4";
            case Type::Color:
                return "Color";
            case Type::ObjectPtr:
                return "Object*";
            case Type::Array:
                return "Array";
            case Type::Dictionary:
                return "Dictionary";
        }
        return "<unknown>";
    }

    // ---------------------------------------------------------------------------
    // Accessors
    // ---------------------------------------------------------------------------

    bool Variant::as_bool() const noexcept
    {
        switch (type_)
        {
            case Type::Bool:
                return storage_.b;
            case Type::Int:
                return storage_.i != 0;
            case Type::Float:
                return storage_.f != 0.0;
            default:
                return false;
        }
    }

    i64 Variant::as_int() const noexcept
    {
        switch (type_)
        {
            case Type::Bool:
                return storage_.b ? 1 : 0;
            case Type::Int:
                return storage_.i;
            case Type::Float:
                return static_cast<i64>(storage_.f);
            default:
                return 0;
        }
    }

    f64 Variant::as_float() const noexcept
    {
        switch (type_)
        {
            case Type::Bool:
                return storage_.b ? 1.0 : 0.0;
            case Type::Int:
                return static_cast<f64>(storage_.i);
            case Type::Float:
                return storage_.f;
            default:
                return 0.0;
        }
    }

    const std::string& Variant::as_string() const
    {
        static const std::string empty;
        return type_ == Type::String ? *storage_.str : empty;
    }

    const Vector2& Variant::as_vector2() const noexcept
    {
        static const Vector2 zero;
        return type_ == Type::Vector2 ? storage_.v2 : zero;
    }

    const Vector3& Variant::as_vector3() const noexcept
    {
        static const Vector3 zero;
        return type_ == Type::Vector3 ? storage_.v3 : zero;
    }

    const Vector4& Variant::as_vector4() const noexcept
    {
        static const Vector4 zero;
        return (type_ == Type::Vector4 || type_ == Type::Color) ? storage_.v4 : zero;
    }

    const Quat& Variant::as_quat() const noexcept
    {
        static const Quat identity;
        return type_ == Type::Quat ? storage_.q : identity;
    }

    const Mat4& Variant::as_mat4() const noexcept
    {
        static const Mat4 identity;
        return type_ == Type::Mat4 ? storage_.m4 : identity;
    }

    Object* Variant::as_object() const noexcept
    {
        return type_ == Type::ObjectPtr ? storage_.obj : nullptr;
    }

    const VariantArray& Variant::as_array() const
    {
        static const VariantArray empty;
        return type_ == Type::Array ? *storage_.arr : empty;
    }

    const VariantDict& Variant::as_dict() const
    {
        static const VariantDict empty;
        return type_ == Type::Dictionary ? *storage_.dict : empty;
    }

    VariantArray& Variant::array_mut()
    {
        if (type_ != Type::Array)
        {
            clear();
            type_ = Type::Array;
            new (&storage_.arr) std::shared_ptr<VariantArray>(std::make_shared<VariantArray>());
        }
        return *storage_.arr;
    }

    VariantDict& Variant::dict_mut()
    {
        if (type_ != Type::Dictionary)
        {
            clear();
            type_ = Type::Dictionary;
            new (&storage_.dict) std::shared_ptr<VariantDict>(std::make_shared<VariantDict>());
        }
        return *storage_.dict;
    }

    // ---------------------------------------------------------------------------
    // Comparison
    // ---------------------------------------------------------------------------

    bool Variant::operator==(const Variant& rhs) const noexcept
    {
        if (type_ != rhs.type_)
        {
            return false;
        }
        switch (type_)
        {
            case Type::Nil:
                return true;
            case Type::Bool:
                return storage_.b == rhs.storage_.b;
            case Type::Int:
                return storage_.i == rhs.storage_.i;
            case Type::Float:
                return storage_.f == rhs.storage_.f;
            case Type::String:
                return *storage_.str == *rhs.storage_.str;
            case Type::Vector2:
                return storage_.v2 == rhs.storage_.v2;
            case Type::Vector3:
                return storage_.v3 == rhs.storage_.v3;
            case Type::Vector4:
            case Type::Color:
                return storage_.v4 == rhs.storage_.v4;
            case Type::Quat:
                return storage_.q == rhs.storage_.q;
            case Type::Mat4:
                return storage_.m4 == rhs.storage_.m4;
            case Type::ObjectPtr:
                return storage_.obj == rhs.storage_.obj;
            case Type::Array:
                return *storage_.arr == *rhs.storage_.arr;
            case Type::Dictionary:
                return *storage_.dict == *rhs.storage_.dict;
        }
        return false;
    }

    usize Variant::hash() const noexcept
    {
        switch (type_)
        {
            case Type::Nil:
                return 0;
            case Type::Bool:
                return std::hash<bool>{}(storage_.b);
            case Type::Int:
                return std::hash<i64>{}(storage_.i);
            case Type::Float:
                return std::hash<f64>{}(storage_.f);
            case Type::String:
                return std::hash<std::string>{}(*storage_.str);
            default:
                return static_cast<usize>(type_);
        }
    }

} // namespace rover
