#pragma once

#include "core/typedefs.h"

#include <functional>
#include <string>

namespace rover
{

    class Object;
    class Variant;

    using StringName = std::string;

    // PropertyType mirrors `Variant::Type` so that Variant-aware code paths can
    // trivially convert between the two enums. Keep these aligned.
    enum class PropertyType : u8
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
        Object,
        Array,
        Dictionary,
        Unknown = 0xFF,
    };

    // Read / write callbacks used by the Inspector and serializer. Both take the
    // owning `Object*` so a single PropertyInfo can be reused across instances.
    using PropertyGetter = std::function<Variant(const Object*)>;
    using PropertySetter = std::function<void(Object*, const Variant&)>;

    struct PropertyInfo
    {
        StringName     name;
        PropertyType   type = PropertyType::Unknown;
        StringName     hint;
        PropertyGetter getter;
        PropertySetter setter;

        [[nodiscard]] bool readable() const noexcept { return static_cast<bool>(getter); }

        [[nodiscard]] bool writable() const noexcept { return static_cast<bool>(setter); }
    };

} // namespace rover
