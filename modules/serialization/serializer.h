#pragma once

#include "core/typedefs.h"
#include "core/variant/variant.h"

#include <string>

namespace rover
{

    // ---------------------------------------------------------------------------
    // Serializer / Deserializer: format-agnostic visitor pair.
    //
    // Concrete formats (JSON text, binary, future: KDL / RON) implement these
    // pure-virtual interfaces. Higher-level code (scene serializer, asset
    // registry) writes against the abstract API and chooses the implementation
    // at the call site.
    // ---------------------------------------------------------------------------

    class Serializer
    {
    public:
        virtual ~Serializer() = default;

        // ---- Object scope (key/value pairs) ----
        virtual void begin_object()               = 0;
        virtual void end_object()                 = 0;
        virtual void key(const std::string& name) = 0;

        // ---- Array scope (positional items) ----
        virtual void begin_array() = 0;
        virtual void end_array()   = 0;

        // ---- Scalar / variant writes ----
        virtual void write_bool(bool v)                 = 0;
        virtual void write_int(i64 v)                   = 0;
        virtual void write_float(f64 v)                 = 0;
        virtual void write_string(const std::string& v) = 0;
        virtual void write_variant(const Variant& v)    = 0;
        virtual void write_null()                       = 0;

        // ---- Finalization ----
        // Returns the serialized payload (text or binary) and resets the
        // serializer's internal buffer.
        [[nodiscard]] virtual std::string take_output() = 0;
    };

    class Deserializer
    {
    public:
        virtual ~Deserializer() = default;

        // Loads source data. Returns true if the input parses cleanly.
        virtual bool load(const std::string& source) = 0;

        // After load, the deserializer exposes the parsed top-level value as a
        // `Variant`. Maps -> VariantDict, arrays -> VariantArray. Caller can
        // walk the result with the helpers in core/variant/variant.h.
        [[nodiscard]] virtual const Variant& root() const = 0;
    };

} // namespace rover
