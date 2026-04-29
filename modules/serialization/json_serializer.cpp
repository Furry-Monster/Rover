#include "modules/serialization/json_serializer.h"

#include <cstdio>

namespace rover
{

    JsonSerializer::JsonSerializer() = default;

    void JsonSerializer::write_indent()
    {
        if (!pretty_)
        {
            return;
        }
        for (u32 i = 0; i < indent_; ++i)
        {
            out_.put(' ');
        }
    }

    void JsonSerializer::maybe_separator()
    {
        if (stack_.empty())
        {
            return;
        }
        auto& top = stack_.back();
        if (top.kind == Frame::Array)
        {
            if (top.element_count > 0)
            {
                out_.put(',');
                if (pretty_)
                {
                    out_.put('\n');
                }
                write_indent();
            }
            else
            {
                if (pretty_)
                {
                    out_.put('\n');
                    write_indent();
                }
            }
            ++top.element_count;
        }
        else
        {
            // Object: a comma is emitted right after the previous value (not
            // after a key). When we land here while expecting_value == true, we
            // are between `key()` and the value -- no comma needed.
            if (top.expecting_value)
            {
                top.expecting_value = false;
                return;
            }
            if (top.element_count > 0)
            {
                out_.put(',');
                if (pretty_)
                {
                    out_.put('\n');
                }
                write_indent();
            }
            else
            {
                if (pretty_)
                {
                    out_.put('\n');
                    write_indent();
                }
            }
            ++top.element_count;
        }
    }

    void JsonSerializer::write_string_quoted(const std::string& s)
    {
        out_.put('"');
        for (char c : s)
        {
            switch (c)
            {
                case '"':
                    out_ << "\\\"";
                    break;
                case '\\':
                    out_ << "\\\\";
                    break;
                case '\n':
                    out_ << "\\n";
                    break;
                case '\r':
                    out_ << "\\r";
                    break;
                case '\t':
                    out_ << "\\t";
                    break;
                default:
                    out_.put(c);
                    break;
            }
        }
        out_.put('"');
    }

    void JsonSerializer::begin_object()
    {
        maybe_separator();
        out_.put('{');
        indent_ += 2;
        stack_.push_back({Frame::Object});
    }

    void JsonSerializer::end_object()
    {
        indent_ -= 2;
        if (!stack_.empty() && stack_.back().element_count > 0 && pretty_)
        {
            out_.put('\n');
            write_indent();
        }
        out_.put('}');
        if (!stack_.empty())
        {
            stack_.pop_back();
        }
    }

    void JsonSerializer::begin_array()
    {
        maybe_separator();
        out_.put('[');
        indent_ += 2;
        stack_.push_back({Frame::Array});
    }

    void JsonSerializer::end_array()
    {
        indent_ -= 2;
        if (!stack_.empty() && stack_.back().element_count > 0 && pretty_)
        {
            out_.put('\n');
            write_indent();
        }
        out_.put(']');
        if (!stack_.empty())
        {
            stack_.pop_back();
        }
    }

    void JsonSerializer::key(const std::string& name)
    {
        if (!stack_.empty() && stack_.back().kind == Frame::Object)
        {
            // Same-frame separator before a new key.
            if (stack_.back().element_count > 0)
            {
                out_.put(',');
                if (pretty_)
                {
                    out_.put('\n');
                }
                write_indent();
            }
            else
            {
                if (pretty_)
                {
                    out_.put('\n');
                    write_indent();
                }
            }
            write_string_quoted(name);
            out_.put(':');
            if (pretty_)
            {
                out_.put(' ');
            }
            stack_.back().expecting_value = true;
            ++stack_.back().element_count;
        }
    }

    void JsonSerializer::write_bool(bool v)
    {
        maybe_separator();
        out_ << (v ? "true" : "false");
    }

    void JsonSerializer::write_int(i64 v)
    {
        maybe_separator();
        out_ << v;
    }

    void JsonSerializer::write_float(f64 v)
    {
        maybe_separator();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", v);
        out_ << buf;
    }

    void JsonSerializer::write_string(const std::string& v)
    {
        maybe_separator();
        write_string_quoted(v);
    }

    void JsonSerializer::write_null()
    {
        maybe_separator();
        out_ << "null";
    }

    void JsonSerializer::write_variant(const Variant& v)
    {
        switch (v.type())
        {
            case Variant::Type::Nil:
                write_null();
                break;
            case Variant::Type::Bool:
                write_bool(v.as_bool());
                break;
            case Variant::Type::Int:
                write_int(v.as_int());
                break;
            case Variant::Type::Float:
                write_float(v.as_float());
                break;
            case Variant::Type::String:
                write_string(v.as_string());
                break;
            case Variant::Type::Vector2:
            {
                const auto& vv = v.as_vector2();
                begin_object();
                key("x");
                write_float(vv.x());
                key("y");
                write_float(vv.y());
                end_object();
                break;
            }
            case Variant::Type::Vector3:
            {
                const auto& vv = v.as_vector3();
                begin_object();
                key("x");
                write_float(vv.x());
                key("y");
                write_float(vv.y());
                key("z");
                write_float(vv.z());
                end_object();
                break;
            }
            case Variant::Type::Vector4:
            case Variant::Type::Color:
            {
                const auto& vv = v.as_vector4();
                begin_object();
                key("x");
                write_float(vv.x());
                key("y");
                write_float(vv.y());
                key("z");
                write_float(vv.z());
                key("w");
                write_float(vv.w());
                end_object();
                break;
            }
            case Variant::Type::Quat:
            {
                const auto& q = v.as_quat();
                begin_object();
                key("w");
                write_float(q.w());
                key("x");
                write_float(q.x());
                key("y");
                write_float(q.y());
                key("z");
                write_float(q.z());
                end_object();
                break;
            }
            case Variant::Type::Mat4:
            {
                const auto& m = v.as_mat4();
                begin_array();
                for (i32 col = 0; col < 4; ++col)
                {
                    begin_array();
                    for (i32 row = 0; row < 4; ++row)
                    {
                        write_float(m(col, row));
                    }
                    end_array();
                }
                end_array();
                break;
            }
            case Variant::Type::Array:
            {
                begin_array();
                for (const auto& item : v.as_array())
                {
                    write_variant(item);
                }
                end_array();
                break;
            }
            case Variant::Type::Dictionary:
            {
                begin_object();
                for (const auto& [k, val] : v.as_dict())
                {
                    key(k);
                    write_variant(val);
                }
                end_object();
                break;
            }
            case Variant::Type::ObjectPtr:
                // Object pointers are runtime-only; serialization expects callers
                // to convert them to references before writing.
                write_null();
                break;
        }
    }

    std::string JsonSerializer::take_output()
    {
        auto s = out_.str();
        out_.str({});
        out_.clear();
        indent_ = 0;
        stack_.clear();
        return s;
    }

} // namespace rover
