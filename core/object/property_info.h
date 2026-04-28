#pragma once

#include "core/typedefs.h"

#include <string>

namespace rover {

using StringName = std::string;

enum class PropertyType : u8 {
    Bool,
    Int,
    Float,
    String,
    Vector2,
    Vector3,
    Vector4,
    Quat,
    Mat4,
    Object,
    Unknown
};

struct PropertyInfo {
    StringName name;
    PropertyType type = PropertyType::Unknown;
    StringName hint;
};

} // namespace rover
