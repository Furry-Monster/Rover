#pragma once

#include "modules/serialization/serializer.h"

#include <sstream>
#include <vector>

namespace rover
{

    // ---------------------------------------------------------------------------
    // JsonSerializer: minimal, indented JSON-like text writer.
    //
    // Targets a strict subset of JSON sufficient for engine assets:
    //   - bool / int / float / null
    //   - string with `"` and `\` escaping
    //   - object  ({ "k": v, ... })
    //   - array   ([ v, ... ])
    //
    // Variant types map as follows:
    //   - Vector{2,3,4} -> {"x":..,"y":.., (..)}
    //   - Quat          -> {"w":..,"x":..,"y":..,"z":..}
    //   - Mat4          -> [ [..], [..], [..], [..] ] (column-major rows)
    //   - Color         -> {"r":..,"g":..,"b":..,"a":..}
    //
    // This writer intentionally does *not* aim for full JSON spec compliance
    // (no NaN/Inf handling, no Unicode escapes); it's a Phase 2 stop-gap until
    // a vendored library is approved per ADR-0008.
    // ---------------------------------------------------------------------------
    class JsonSerializer final : public Serializer
    {
    public:
        JsonSerializer();

        void begin_object() override;
        void end_object() override;
        void key(const std::string& name) override;
        void begin_array() override;
        void end_array() override;

        void write_bool(bool v) override;
        void write_int(i64 v) override;
        void write_float(f64 v) override;
        void write_string(const std::string& v) override;
        void write_variant(const Variant& v) override;
        void write_null() override;

        [[nodiscard]] std::string take_output() override;

    private:
        // Writes a comma if the current container already has at least one
        // element. Updates the per-frame counter so subsequent commits append a
        // separator.
        void maybe_separator();
        void write_indent();
        void write_string_quoted(const std::string& s);

        enum class Frame : u8
        {
            Object,
            Array
        };

        struct FrameState
        {
            Frame kind;
            u32   element_count   = 0;
            bool  expecting_value = false; // object: just emitted a key
        };

        std::ostringstream      out_;
        std::vector<FrameState> stack_;
        bool                    pretty_ = true;
        u32                     indent_ = 0;
    };

} // namespace rover
