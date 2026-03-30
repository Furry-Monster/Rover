#include "core/variant/variant.h"

#include <cstring>
#include <new>
#include <sstream>

// ---------------------------------------------------------------------------
// Helpers for inline-stored math types
// ---------------------------------------------------------------------------

template <typename T>
static void
_construct_in(uint8_t* mem, const T& val)
{
    static_assert(sizeof(T) <= sizeof(real_t) * 4);
    new (mem) T(val);
}

template <typename T>
static T&
_get_from(uint8_t* mem)
{
    return *reinterpret_cast<T*>(mem);
}

template <typename T>
static const T&
_get_from(const uint8_t* mem)
{
    return *reinterpret_cast<const T*>(mem);
}

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

Variant::Variant(bool p_val) : _type(BOOL)
{
    _data._bool = p_val;
}

Variant::Variant(int32_t p_val) : _type(INT)
{
    _data._int = p_val;
}

Variant::Variant(int64_t p_val) : _type(INT)
{
    _data._int = p_val;
}

Variant::Variant(float p_val) : _type(FLOAT)
{
    _data._float = static_cast<double>(p_val);
}

Variant::Variant(double p_val) : _type(FLOAT)
{
    _data._float = p_val;
}

Variant::Variant(const char* p_val) : _type(STRING)
{
    _data._ptr = new std::string(p_val ? p_val : "");
}

Variant::Variant(const std::string& p_val) : _type(STRING)
{
    _data._ptr = new std::string(p_val);
}

Variant::Variant(const StringName& p_val) : _type(STRING)
{
    _data._ptr = new std::string(p_val.c_str());
}

Variant::Variant(const Vector2& p_val) : _type(VECTOR2)
{
    _construct_in(_data._mem, p_val);
}

Variant::Variant(const Vector3& p_val) : _type(VECTOR3)
{
    _construct_in(_data._mem, p_val);
}

Variant::Variant(const Vector4& p_val) : _type(VECTOR4)
{
    _construct_in(_data._mem, p_val);
}

Variant::Variant(const Color& p_val) : _type(COLOR)
{
    _construct_in(_data._mem, p_val);
}

Variant::Variant(Object* p_val) : _type(p_val ? OBJECT : NIL)
{
    _data._obj = p_val;
}

// ---------------------------------------------------------------------------
// Copy / Move / Destruct
// ---------------------------------------------------------------------------

Variant::Variant(const Variant& p_other)
{
    _copy_from(p_other);
}

Variant::Variant(Variant&& p_other) noexcept : _type(p_other._type), _data(p_other._data)
{
    p_other._type      = NIL;
    p_other._data._ptr = nullptr;
}

Variant::~Variant()
{
    _clear();
}

Variant&
Variant::operator=(const Variant& p_other)
{
    if (this != &p_other)
    {
        _clear();
        _copy_from(p_other);
    }
    return *this;
}

Variant&
Variant::operator=(Variant&& p_other) noexcept
{
    if (this != &p_other)
    {
        _clear();
        _type              = p_other._type;
        _data              = p_other._data;
        p_other._type      = NIL;
        p_other._data._ptr = nullptr;
    }
    return *this;
}

void
Variant::_clear()
{
    switch (_type)
    {
        case STRING:
            delete static_cast<std::string*>(_data._ptr);
            break;
        case ARRAY:
            delete static_cast<VariantArray*>(_data._ptr);
            break;
        case DICTIONARY:
            delete static_cast<VariantDictionary*>(_data._ptr);
            break;
        default:
            break;
    }
    _type      = NIL;
    _data._ptr = nullptr;
}

void
Variant::_copy_from(const Variant& p_other)
{
    _type = p_other._type;

    switch (_type)
    {
        case NIL:
            break;
        case BOOL:
            _data._bool = p_other._data._bool;
            break;
        case INT:
            _data._int = p_other._data._int;
            break;
        case FLOAT:
            _data._float = p_other._data._float;
            break;
        case STRING:
            _data._ptr = new std::string(*static_cast<const std::string*>(p_other._data._ptr));
            break;
        case VECTOR2:
            _construct_in(_data._mem, _get_from<Vector2>(p_other._data._mem));
            break;
        case VECTOR3:
            _construct_in(_data._mem, _get_from<Vector3>(p_other._data._mem));
            break;
        case VECTOR4:
            _construct_in(_data._mem, _get_from<Vector4>(p_other._data._mem));
            break;
        case COLOR:
            _construct_in(_data._mem, _get_from<Color>(p_other._data._mem));
            break;
        case OBJECT:
            _data._obj = p_other._data._obj;
            break;
        case ARRAY:
            _data._ptr = new VariantArray(*static_cast<const VariantArray*>(p_other._data._ptr));
            break;
        case DICTIONARY:
            _data._ptr = new VariantDictionary(*static_cast<const VariantDictionary*>(p_other._data._ptr));
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Type name
// ---------------------------------------------------------------------------

const char*
Variant::get_type_name(Type p_type)
{
    // clang-format off
    static const char* names[TYPE_MAX] = {
        "Nil", "Bool", "Int", "Float", "String",
        "Vector2", "Vector3", "Vector4", "Color",
        "Object", "Array", "Dictionary",
    };
    // clang-format on
    if (p_type < 0 || p_type >= TYPE_MAX)
    {
        return "Unknown";
    }
    return names[p_type];
}

// ---------------------------------------------------------------------------
// operator bool
// ---------------------------------------------------------------------------

Variant::operator bool() const
{
    switch (_type)
    {
        case NIL:
            return false;
        case BOOL:
            return _data._bool;
        case INT:
            return _data._int != 0;
        case FLOAT:
            return _data._float != 0.0;
        case STRING:
            return !static_cast<const std::string*>(_data._ptr)->empty();
        case OBJECT:
            return _data._obj != nullptr;
        default:
            return true;
    }
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------

bool
Variant::as_bool() const
{
    switch (_type)
    {
        case BOOL:
            return _data._bool;
        case INT:
            return _data._int != 0;
        case FLOAT:
            return _data._float != 0.0;
        default:
            return false;
    }
}

int64_t
Variant::as_int() const
{
    switch (_type)
    {
        case BOOL:
            return _data._bool ? 1 : 0;
        case INT:
            return _data._int;
        case FLOAT:
            return static_cast<int64_t>(_data._float);
        default:
            return 0;
    }
}

double
Variant::as_float() const
{
    switch (_type)
    {
        case BOOL:
            return _data._bool ? 1.0 : 0.0;
        case INT:
            return static_cast<double>(_data._int);
        case FLOAT:
            return _data._float;
        default:
            return 0.0;
    }
}

std::string
Variant::as_string() const
{
    switch (_type)
    {
        case NIL:
            return "null";
        case BOOL:
            return _data._bool ? "true" : "false";
        case INT:
            return std::to_string(_data._int);
        case FLOAT:
            return std::to_string(_data._float);
        case STRING:
            return *static_cast<const std::string*>(_data._ptr);
        case VECTOR2:
        {
            const auto&        v = _get_from<Vector2>(_data._mem);
            std::ostringstream ss;
            ss << "(" << v.x << ", " << v.y << ")";
            return ss.str();
        }
        case VECTOR3:
        {
            const auto&        v = _get_from<Vector3>(_data._mem);
            std::ostringstream ss;
            ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
            return ss.str();
        }
        case VECTOR4:
        {
            const auto&        v = _get_from<Vector4>(_data._mem);
            std::ostringstream ss;
            ss << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
            return ss.str();
        }
        case COLOR:
        {
            const auto&        c = _get_from<Color>(_data._mem);
            std::ostringstream ss;
            ss << "Color(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
            return ss.str();
        }
        case OBJECT:
            return _data._obj ? "<Object>" : "null";
        case ARRAY:
            return "<Array>";
        case DICTIONARY:
            return "<Dictionary>";
        default:
            return "";
    }
}

Vector2
Variant::as_vector2() const
{
    if (_type == VECTOR2)
    {
        return _get_from<Vector2>(_data._mem);
    }
    return {};
}

Vector3
Variant::as_vector3() const
{
    if (_type == VECTOR3)
    {
        return _get_from<Vector3>(_data._mem);
    }
    return {};
}

Vector4
Variant::as_vector4() const
{
    if (_type == VECTOR4)
    {
        return _get_from<Vector4>(_data._mem);
    }
    return {};
}

Color
Variant::as_color() const
{
    if (_type == COLOR)
    {
        return _get_from<Color>(_data._mem);
    }
    return {};
}

Object*
Variant::as_object() const
{
    if (_type == OBJECT)
    {
        return _data._obj;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

bool
Variant::operator==(const Variant& p_other) const
{
    if (_type != p_other._type)
    {
        if (is_num() && p_other.is_num())
        {
            return as_float() == p_other.as_float();
        }
        return false;
    }

    switch (_type)
    {
        case NIL:
            return true;
        case BOOL:
            return _data._bool == p_other._data._bool;
        case INT:
            return _data._int == p_other._data._int;
        case FLOAT:
            return _data._float == p_other._data._float;
        case STRING:
            return *static_cast<const std::string*>(_data._ptr) == *static_cast<const std::string*>(p_other._data._ptr);
        case VECTOR2:
            return _get_from<Vector2>(_data._mem) == _get_from<Vector2>(p_other._data._mem);
        case VECTOR3:
            return _get_from<Vector3>(_data._mem) == _get_from<Vector3>(p_other._data._mem);
        case VECTOR4:
            return _get_from<Vector4>(_data._mem) == _get_from<Vector4>(p_other._data._mem);
        case COLOR:
            return _get_from<Color>(_data._mem) == _get_from<Color>(p_other._data._mem);
        case OBJECT:
            return _data._obj == p_other._data._obj;
        default:
            return false;
    }
}

bool
Variant::operator!=(const Variant& p_other) const
{
    return !(*this == p_other);
}

// ---------------------------------------------------------------------------
// Hash
// ---------------------------------------------------------------------------

static uint32_t
hash_fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

uint32_t
Variant::hash() const
{
    switch (_type)
    {
        case NIL:
            return 0;
        case BOOL:
            return _data._bool ? 1 : 0;
        case INT:
            return hash_fmix32(static_cast<uint32_t>(_data._int ^ (_data._int >> 32)));
        case FLOAT:
        {
            uint64_t bits;
            std::memcpy(&bits, &_data._float, sizeof(bits));
            return hash_fmix32(static_cast<uint32_t>(bits ^ (bits >> 32)));
        }
        case STRING:
        {
            const auto& s = *static_cast<const std::string*>(_data._ptr);
            uint32_t    h = 2166136261u;
            for (char c : s)
            {
                h ^= static_cast<uint8_t>(c);
                h *= 16777619u;
            }
            return h;
        }
        case VECTOR2:
        {
            const auto& v = _get_from<Vector2>(_data._mem);
            uint32_t    h = 0;
            std::memcpy(&h, &v.x, sizeof(real_t));
            uint32_t h2 = 0;
            std::memcpy(&h2, &v.y, sizeof(real_t));
            return hash_fmix32(h ^ (h2 * 2654435761u));
        }
        case VECTOR3:
        {
            const auto& v = _get_from<Vector3>(_data._mem);
            uint32_t    h = 0;
            std::memcpy(&h, &v.x, sizeof(real_t));
            uint32_t h2 = 0;
            std::memcpy(&h2, &v.y, sizeof(real_t));
            uint32_t h3 = 0;
            std::memcpy(&h3, &v.z, sizeof(real_t));
            return hash_fmix32(h ^ (h2 * 2654435761u) ^ (h3 * 2246822519u));
        }
        case OBJECT:
            return hash_fmix32(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(_data._obj)));
        default:
            return 0;
    }
}
