#include "modules/serialization/json_deserializer.h"

#include <cctype>
#include <cstdlib>
#include <utility>

namespace rover
{

    JsonDeserializer::JsonDeserializer() = default;

    bool JsonDeserializer::load(const std::string& source)
    {
        src_  = &source;
        pos_  = 0;
        root_ = Variant{};
        skip_whitespace();
        return parse_value(root_);
    }

    char JsonDeserializer::peek() const
    {
        return (src_ && pos_ < src_->size()) ? (*src_)[pos_] : '\0';
    }

    char JsonDeserializer::consume()
    {
        if (!src_ || pos_ >= src_->size())
        {
            return '\0';
        }
        return (*src_)[pos_++];
    }

    void JsonDeserializer::skip_whitespace()
    {
        while (pos_ < src_->size())
        {
            const char c = (*src_)[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                ++pos_;
            }
            else
            {
                break;
            }
        }
    }

    bool JsonDeserializer::parse_literal(const char* literal)
    {
        const usize start = pos_;
        while (*literal)
        {
            if (peek() != *literal)
            {
                pos_ = start;
                return false;
            }
            consume();
            ++literal;
        }
        return true;
    }

    bool JsonDeserializer::parse_string(std::string& out)
    {
        if (peek() != '"')
        {
            return false;
        }
        consume();
        out.clear();
        while (pos_ < src_->size())
        {
            const char c = consume();
            if (c == '"')
            {
                return true;
            }
            if (c == '\\')
            {
                const char e = consume();
                switch (e)
                {
                    case '"':
                        out.push_back('"');
                        break;
                    case '\\':
                        out.push_back('\\');
                        break;
                    case '/':
                        out.push_back('/');
                        break;
                    case 'n':
                        out.push_back('\n');
                        break;
                    case 'r':
                        out.push_back('\r');
                        break;
                    case 't':
                        out.push_back('\t');
                        break;
                    default:
                        out.push_back(e);
                        break; // \u not supported
                }
            }
            else
            {
                out.push_back(c);
            }
        }
        return false;
    }

    bool JsonDeserializer::parse_number(Variant& out)
    {
        const usize start    = pos_;
        bool        is_float = false;
        if (peek() == '-' || peek() == '+')
        {
            consume();
        }
        while (pos_ < src_->size())
        {
            const char c = peek();
            if (std::isdigit(static_cast<unsigned char>(c)))
            {
                consume();
            }
            else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-')
            {
                is_float = true;
                consume();
            }
            else
            {
                break;
            }
        }
        if (start == pos_)
        {
            return false;
        }
        const std::string token = src_->substr(start, pos_ - start);
        if (is_float)
        {
            out = Variant{std::strtod(token.c_str(), nullptr)};
        }
        else
        {
            out = Variant{static_cast<i64>(std::strtoll(token.c_str(), nullptr, 10))};
        }
        return true;
    }

    bool JsonDeserializer::parse_array(Variant& out)
    {
        if (peek() != '[')
        {
            return false;
        }
        consume();
        VariantArray arr;
        skip_whitespace();
        if (peek() == ']')
        {
            consume();
            out = Variant{std::move(arr)};
            return true;
        }
        while (true)
        {
            skip_whitespace();
            Variant item;
            if (!parse_value(item))
            {
                return false;
            }
            arr.push_back(std::move(item));
            skip_whitespace();
            const char c = peek();
            if (c == ',')
            {
                consume();
                continue;
            }
            if (c == ']')
            {
                consume();
                break;
            }
            return false;
        }
        out = Variant{std::move(arr)};
        return true;
    }

    bool JsonDeserializer::parse_object(Variant& out)
    {
        if (peek() != '{')
        {
            return false;
        }
        consume();
        VariantDict dict;
        skip_whitespace();
        if (peek() == '}')
        {
            consume();
            out = Variant{std::move(dict)};
            return true;
        }
        while (true)
        {
            skip_whitespace();
            std::string k;
            if (!parse_string(k))
            {
                return false;
            }
            skip_whitespace();
            if (peek() != ':')
            {
                return false;
            }
            consume();
            skip_whitespace();
            Variant val;
            if (!parse_value(val))
            {
                return false;
            }
            dict.emplace(std::move(k), std::move(val));
            skip_whitespace();
            const char c = peek();
            if (c == ',')
            {
                consume();
                continue;
            }
            if (c == '}')
            {
                consume();
                break;
            }
            return false;
        }
        out = Variant{std::move(dict)};
        return true;
    }

    bool JsonDeserializer::parse_value(Variant& out)
    {
        skip_whitespace();
        const char c = peek();
        if (c == '\0')
        {
            return false;
        }
        if (c == '{')
        {
            return parse_object(out);
        }
        if (c == '[')
        {
            return parse_array(out);
        }
        if (c == '"')
        {
            std::string s;
            if (!parse_string(s))
            {
                return false;
            }
            out = Variant{std::move(s)};
            return true;
        }
        if (c == 't')
        {
            if (parse_literal("true"))
            {
                out = Variant{true};
                return true;
            }
            return false;
        }
        if (c == 'f')
        {
            if (parse_literal("false"))
            {
                out = Variant{false};
                return true;
            }
            return false;
        }
        if (c == 'n')
        {
            if (parse_literal("null"))
            {
                out = Variant{};
                return true;
            }
            return false;
        }
        if (c == '-' || c == '+' || std::isdigit(static_cast<unsigned char>(c)))
        {
            return parse_number(out);
        }
        return false;
    }

} // namespace rover
