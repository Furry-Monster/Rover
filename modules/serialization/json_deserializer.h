#pragma once

#include "modules/serialization/serializer.h"

namespace rover
{

    // JsonDeserializer parses the (small) JSON subset emitted by JsonSerializer
    // into a `Variant`. Unknown / malformed input fails `load()` cleanly. The
    // returned root is a Dict / Array / scalar that callers can introspect.
    class JsonDeserializer final : public Deserializer
    {
    public:
        JsonDeserializer();

        bool load(const std::string& source) override;

        [[nodiscard]] const Variant& root() const override { return root_; }

    private:
        bool parse_value(Variant& out);
        bool parse_object(Variant& out);
        bool parse_array(Variant& out);
        bool parse_string(std::string& out);
        bool parse_number(Variant& out);
        bool parse_literal(const char* literal);
        void skip_whitespace();

        [[nodiscard]] char peek() const;
        char               consume();

        const std::string* src_ = nullptr;
        usize              pos_ = 0;
        Variant            root_;
    };

} // namespace rover
