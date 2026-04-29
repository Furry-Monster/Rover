#pragma once

#include "modules/serialization/serializer.h"

#include <string>
#include <vector>

namespace rover
{

    // ---------------------------------------------------------------------------
    // BinarySerializer / BinaryDeserializer: compact little-endian binary format.
    //
    // Format header:
    //   magic   : 4 bytes  "RBIN"
    //   version : u32      = 1
    //   payload : tagged values (see TypeTag enum below)
    //
    // Each value:
    //   tag     : u8 (TypeTag)
    //   data    : type-specific
    //     scalars use little-endian fixed widths
    //     strings use u32 length + bytes
    //     arrays  use u32 count + values...
    //     dicts   use u32 count + (string key + value)...
    //
    // Object/Array scope APIs map onto Dict and Array variants respectively.
    // `key` followed by a value is paired into the most recent Dict frame.
    // ---------------------------------------------------------------------------

    class BinarySerializer final : public Serializer
    {
    public:
        BinarySerializer();

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
        // We build the structured tree first, then encode at `take_output`. This
        // keeps the API symmetric with the JSON serializer (no need for size
        // back-patching during writes).
        void push_value(Variant v);

        enum class FrameKind : u8
        {
            Dict,
            Array
        };

        struct Frame
        {
            FrameKind    kind;
            std::string  pending_key; // for Dict: most recent key
            bool         has_pending_key = false;
            VariantArray array_items;
            VariantDict  dict_items;
        };

        std::vector<Frame> stack_;
        Variant            root_;
    };

    class BinaryDeserializer final : public Deserializer
    {
    public:
        BinaryDeserializer();

        bool load(const std::string& source) override;

        [[nodiscard]] const Variant& root() const override { return root_; }

    private:
        bool read_value(Variant& out);
        bool read_bytes(void* dst, usize n);

        const std::string* src_ = nullptr;
        usize              pos_ = 0;
        Variant            root_;
    };

} // namespace rover
