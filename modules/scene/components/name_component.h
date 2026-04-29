#pragma once

#include <string>
#include <utility>

namespace rover
{

    // Optional human-readable name for an entity. Used by the editor's
    // scene-tree view and by the serializer for stable references.
    struct NameComponent
    {
        std::string name;

        NameComponent() = default;

        explicit NameComponent(std::string n) : name(std::move(n)) {}
    };

} // namespace rover
