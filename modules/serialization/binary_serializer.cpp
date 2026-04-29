#include "modules/serialization/binary_serializer.h"

#include <cstring>
#include <utility>

namespace rover
{

    namespace
    {

        constexpr char kMagic[4] = {'R', 'B', 'I', 'N'};
        constexpr u32  kVersion  = 1;

        enum class TypeTag : u8
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
            Array,
            Dictionary,
        };

        void put_u8(std::string& out, u8 v)
        {
            out.push_back(static_cast<char>(v));
        }

        void put_u32(std::string& out, u32 v)
        {
            out.push_back(static_cast<char>(v & 0xFF));
            out.push_back(static_cast<char>((v >> 8) & 0xFF));
            out.push_back(static_cast<char>((v >> 16) & 0xFF));
            out.push_back(static_cast<char>((v >> 24) & 0xFF));
        }

        void put_i64(std::string& out, i64 v)
        {
            const u64 u = static_cast<u64>(v);
            for (int i = 0; i < 8; ++i)
            {
                out.push_back(static_cast<char>((u >> (i * 8)) & 0xFF));
            }
        }

        void put_f32(std::string& out, f32 v)
        {
            u32 bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            put_u32(out, bits);
        }

        void put_f64(std::string& out, f64 v)
        {
            u64 bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            for (int i = 0; i < 8; ++i)
            {
                out.push_back(static_cast<char>((bits >> (i * 8)) & 0xFF));
            }
        }

        void put_string(std::string& out, const std::string& s)
        {
            put_u32(out, static_cast<u32>(s.size()));
            out.append(s);
        }

        void encode(std::string& out, const Variant& v);

        void encode(std::string& out, const Variant& v)
        {
            switch (v.type())
            {
                case Variant::Type::Nil:
                    put_u8(out, static_cast<u8>(TypeTag::Nil));
                    break;
                case Variant::Type::Bool:
                    put_u8(out, static_cast<u8>(TypeTag::Bool));
                    put_u8(out, v.as_bool() ? 1 : 0);
                    break;
                case Variant::Type::Int:
                    put_u8(out, static_cast<u8>(TypeTag::Int));
                    put_i64(out, v.as_int());
                    break;
                case Variant::Type::Float:
                    put_u8(out, static_cast<u8>(TypeTag::Float));
                    put_f64(out, v.as_float());
                    break;
                case Variant::Type::String:
                    put_u8(out, static_cast<u8>(TypeTag::String));
                    put_string(out, v.as_string());
                    break;
                case Variant::Type::Vector2:
                {
                    const auto& vv = v.as_vector2();
                    put_u8(out, static_cast<u8>(TypeTag::Vector2));
                    put_f32(out, vv.x());
                    put_f32(out, vv.y());
                    break;
                }
                case Variant::Type::Vector3:
                {
                    const auto& vv = v.as_vector3();
                    put_u8(out, static_cast<u8>(TypeTag::Vector3));
                    put_f32(out, vv.x());
                    put_f32(out, vv.y());
                    put_f32(out, vv.z());
                    break;
                }
                case Variant::Type::Vector4:
                {
                    const auto& vv = v.as_vector4();
                    put_u8(out, static_cast<u8>(TypeTag::Vector4));
                    put_f32(out, vv.x());
                    put_f32(out, vv.y());
                    put_f32(out, vv.z());
                    put_f32(out, vv.w());
                    break;
                }
                case Variant::Type::Color:
                {
                    const auto& vv = v.as_vector4();
                    put_u8(out, static_cast<u8>(TypeTag::Color));
                    put_f32(out, vv.x());
                    put_f32(out, vv.y());
                    put_f32(out, vv.z());
                    put_f32(out, vv.w());
                    break;
                }
                case Variant::Type::Quat:
                {
                    const auto& q = v.as_quat();
                    put_u8(out, static_cast<u8>(TypeTag::Quat));
                    put_f32(out, q.w());
                    put_f32(out, q.x());
                    put_f32(out, q.y());
                    put_f32(out, q.z());
                    break;
                }
                case Variant::Type::Mat4:
                {
                    const auto& m = v.as_mat4();
                    put_u8(out, static_cast<u8>(TypeTag::Mat4));
                    for (i32 c = 0; c < 4; ++c)
                    {
                        for (i32 r = 0; r < 4; ++r)
                        {
                            put_f32(out, m(c, r));
                        }
                    }
                    break;
                }
                case Variant::Type::Array:
                {
                    put_u8(out, static_cast<u8>(TypeTag::Array));
                    const auto& arr = v.as_array();
                    put_u32(out, static_cast<u32>(arr.size()));
                    for (const auto& item : arr)
                    {
                        encode(out, item);
                    }
                    break;
                }
                case Variant::Type::Dictionary:
                {
                    put_u8(out, static_cast<u8>(TypeTag::Dictionary));
                    const auto& dict = v.as_dict();
                    put_u32(out, static_cast<u32>(dict.size()));
                    for (const auto& [k, val] : dict)
                    {
                        put_string(out, k);
                        encode(out, val);
                    }
                    break;
                }
                case Variant::Type::ObjectPtr:
                    put_u8(out, static_cast<u8>(TypeTag::Nil));
                    break;
            }
        }

        bool read_u8(const std::string& src, usize& pos, u8& out)
        {
            if (pos >= src.size())
            {
                return false;
            }
            out = static_cast<u8>(src[pos++]);
            return true;
        }

        bool read_u32(const std::string& src, usize& pos, u32& out)
        {
            if (pos + 4 > src.size())
            {
                return false;
            }
            out = (static_cast<u32>(static_cast<u8>(src[pos]))) |
                  (static_cast<u32>(static_cast<u8>(src[pos + 1])) << 8) |
                  (static_cast<u32>(static_cast<u8>(src[pos + 2])) << 16) |
                  (static_cast<u32>(static_cast<u8>(src[pos + 3])) << 24);
            pos += 4;
            return true;
        }

        bool read_i64(const std::string& src, usize& pos, i64& out)
        {
            if (pos + 8 > src.size())
            {
                return false;
            }
            u64 u = 0;
            for (int i = 0; i < 8; ++i)
            {
                u |= static_cast<u64>(static_cast<u8>(src[pos + i])) << (i * 8);
            }
            pos += 8;
            out = static_cast<i64>(u);
            return true;
        }

        bool read_f32(const std::string& src, usize& pos, f32& out)
        {
            u32 bits = 0;
            if (!read_u32(src, pos, bits))
            {
                return false;
            }
            std::memcpy(&out, &bits, sizeof(out));
            return true;
        }

        bool read_f64(const std::string& src, usize& pos, f64& out)
        {
            if (pos + 8 > src.size())
            {
                return false;
            }
            u64 u = 0;
            for (int i = 0; i < 8; ++i)
            {
                u |= static_cast<u64>(static_cast<u8>(src[pos + i])) << (i * 8);
            }
            pos += 8;
            std::memcpy(&out, &u, sizeof(out));
            return true;
        }

        bool read_string(const std::string& src, usize& pos, std::string& out)
        {
            u32 n = 0;
            if (!read_u32(src, pos, n))
            {
                return false;
            }
            if (pos + n > src.size())
            {
                return false;
            }
            out.assign(src.data() + pos, n);
            pos += n;
            return true;
        }

    } // namespace

    // ---------------------------------------------------------------------------
    // BinarySerializer
    // ---------------------------------------------------------------------------

    BinarySerializer::BinarySerializer() = default;

    void BinarySerializer::push_value(Variant v)
    {
        if (stack_.empty())
        {
            root_ = std::move(v);
            return;
        }
        auto& top = stack_.back();
        if (top.kind == FrameKind::Array)
        {
            top.array_items.push_back(std::move(v));
        }
        else
        {
            top.dict_items.emplace(std::move(top.pending_key), std::move(v));
            top.has_pending_key = false;
        }
    }

    void BinarySerializer::begin_object()
    {
        stack_.push_back({FrameKind::Dict});
    }

    void BinarySerializer::end_object()
    {
        if (stack_.empty())
        {
            return;
        }
        Frame f = std::move(stack_.back());
        stack_.pop_back();
        push_value(Variant{std::move(f.dict_items)});
    }

    void BinarySerializer::begin_array()
    {
        stack_.push_back({FrameKind::Array});
    }

    void BinarySerializer::end_array()
    {
        if (stack_.empty())
        {
            return;
        }
        Frame f = std::move(stack_.back());
        stack_.pop_back();
        push_value(Variant{std::move(f.array_items)});
    }

    void BinarySerializer::key(const std::string& name)
    {
        if (stack_.empty() || stack_.back().kind != FrameKind::Dict)
        {
            return;
        }
        stack_.back().pending_key     = name;
        stack_.back().has_pending_key = true;
    }

    void BinarySerializer::write_bool(bool v)
    {
        push_value(Variant{v});
    }

    void BinarySerializer::write_int(i64 v)
    {
        push_value(Variant{v});
    }

    void BinarySerializer::write_float(f64 v)
    {
        push_value(Variant{v});
    }

    void BinarySerializer::write_string(const std::string& v)
    {
        push_value(Variant{v});
    }

    void BinarySerializer::write_null()
    {
        push_value(Variant{});
    }

    void BinarySerializer::write_variant(const Variant& v)
    {
        push_value(v);
    }

    std::string BinarySerializer::take_output()
    {
        std::string out;
        out.reserve(64);
        out.append(kMagic, 4);
        put_u32(out, kVersion);
        encode(out, root_);
        root_ = Variant{};
        stack_.clear();
        return out;
    }

    // ---------------------------------------------------------------------------
    // BinaryDeserializer
    // ---------------------------------------------------------------------------

    BinaryDeserializer::BinaryDeserializer() = default;

    bool BinaryDeserializer::read_bytes(void* dst, usize n)
    {
        if (!src_ || pos_ + n > src_->size())
        {
            return false;
        }
        std::memcpy(dst, src_->data() + pos_, n);
        pos_ += n;
        return true;
    }

    bool BinaryDeserializer::read_value(Variant& out)
    {
        u8 tag = 0;
        if (!read_u8(*src_, pos_, tag))
        {
            return false;
        }
        switch (static_cast<TypeTag>(tag))
        {
            case TypeTag::Nil:
                out = Variant{};
                return true;
            case TypeTag::Bool:
            {
                u8 b = 0;
                if (!read_u8(*src_, pos_, b))
                {
                    return false;
                }
                out = Variant{b != 0};
                return true;
            }
            case TypeTag::Int:
            {
                i64 v = 0;
                if (!read_i64(*src_, pos_, v))
                {
                    return false;
                }
                out = Variant{v};
                return true;
            }
            case TypeTag::Float:
            {
                f64 v = 0;
                if (!read_f64(*src_, pos_, v))
                {
                    return false;
                }
                out = Variant{v};
                return true;
            }
            case TypeTag::String:
            {
                std::string s;
                if (!read_string(*src_, pos_, s))
                {
                    return false;
                }
                out = Variant{std::move(s)};
                return true;
            }
            case TypeTag::Vector2:
            {
                f32 x, y;
                if (!read_f32(*src_, pos_, x) || !read_f32(*src_, pos_, y))
                {
                    return false;
                }
                out = Variant{Vector2{x, y}};
                return true;
            }
            case TypeTag::Vector3:
            {
                f32 x, y, z;
                if (!read_f32(*src_, pos_, x) || !read_f32(*src_, pos_, y) || !read_f32(*src_, pos_, z))
                {
                    return false;
                }
                out = Variant{Vector3{x, y, z}};
                return true;
            }
            case TypeTag::Vector4:
            case TypeTag::Color:
            {
                f32 x, y, z, w;
                if (!read_f32(*src_, pos_, x) || !read_f32(*src_, pos_, y) || !read_f32(*src_, pos_, z) ||
                    !read_f32(*src_, pos_, w))
                {
                    return false;
                }
                out = Variant{Vector4{x, y, z, w}};
                return true;
            }
            case TypeTag::Quat:
            {
                f32 w, x, y, z;
                if (!read_f32(*src_, pos_, w) || !read_f32(*src_, pos_, x) || !read_f32(*src_, pos_, y) ||
                    !read_f32(*src_, pos_, z))
                {
                    return false;
                }
                out = Variant{Quat{w, x, y, z}};
                return true;
            }
            case TypeTag::Mat4:
            {
                f32 m[16];
                for (int i = 0; i < 16; ++i)
                {
                    if (!read_f32(*src_, pos_, m[i]))
                    {
                        return false;
                    }
                }
                out = Variant{Mat4{m[0],
                                   m[1],
                                   m[2],
                                   m[3],
                                   m[4],
                                   m[5],
                                   m[6],
                                   m[7],
                                   m[8],
                                   m[9],
                                   m[10],
                                   m[11],
                                   m[12],
                                   m[13],
                                   m[14],
                                   m[15]}};
                return true;
            }
            case TypeTag::Array:
            {
                u32 n = 0;
                if (!read_u32(*src_, pos_, n))
                {
                    return false;
                }
                VariantArray arr;
                arr.reserve(n);
                for (u32 i = 0; i < n; ++i)
                {
                    Variant v;
                    if (!read_value(v))
                    {
                        return false;
                    }
                    arr.push_back(std::move(v));
                }
                out = Variant{std::move(arr)};
                return true;
            }
            case TypeTag::Dictionary:
            {
                u32 n = 0;
                if (!read_u32(*src_, pos_, n))
                {
                    return false;
                }
                VariantDict dict;
                for (u32 i = 0; i < n; ++i)
                {
                    std::string k;
                    if (!read_string(*src_, pos_, k))
                    {
                        return false;
                    }
                    Variant v;
                    if (!read_value(v))
                    {
                        return false;
                    }
                    dict.emplace(std::move(k), std::move(v));
                }
                out = Variant{std::move(dict)};
                return true;
            }
        }
        return false;
    }

    bool BinaryDeserializer::load(const std::string& source)
    {
        src_  = &source;
        pos_  = 0;
        root_ = Variant{};
        if (source.size() < 8)
        {
            return false;
        }
        if (std::memcmp(source.data(), kMagic, 4) != 0)
        {
            return false;
        }
        pos_        = 4;
        u32 version = 0;
        if (!read_u32(source, pos_, version))
        {
            return false;
        }
        if (version != kVersion)
        {
            return false;
        }
        return read_value(root_);
    }

} // namespace rover
