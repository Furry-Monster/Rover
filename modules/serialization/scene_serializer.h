#pragma once

#include "core/variant/variant.h"
#include "modules/scene/world.h"

#include <string>

namespace rover
{

    class Serializer;
    class Deserializer;

    // ---------------------------------------------------------------------------
    // SceneSerializer: writes a `World`'s entities + built-in components into a
    // Serializer (any format) and reads the same shape back into a fresh World.
    //
    // Phase 2 supports the canonical built-in components only:
    //   - NameComponent
    //   - TransformComponent
    //   - CameraComponent
    //   - LightComponent
    //   - MeshComponent (stable mesh_asset path; GPU handles are never stored)
    //   - ParentComponent / ChildrenComponent
    //
    // Custom user components will be wired through `ClassDB::list_properties`
    // in Phase 3 once the property-binding macros mature.
    // ---------------------------------------------------------------------------
    class SceneSerializer
    {
    public:
        static void serialize(World& world, Serializer& out);
        static bool deserialize(const Deserializer& in, World& out);

        // Convenience: build a Variant tree (Dictionary) from a World. Equivalent
        // to `serialize(world, json)` -> parse(json), but skips the round-trip.
        [[nodiscard]] static Variant to_variant(World& world);

        // Replaces the destination only on success. Unsupported versions, invalid
        // component shapes or cyclic parent links fail without changing it.
        static bool from_variant(const Variant& tree, World& out);

        // ---- File-level shortcuts (use core/os/file_access for IO) ----

        // Writes the world to `path` as JSON. Returns true on success.
        static bool save_json(World& world, const std::string& path);
        static bool load_json(const std::string& path, World& out);

        static bool save_binary(World& world, const std::string& path);
        static bool load_binary(const std::string& path, World& out);
    };

} // namespace rover
