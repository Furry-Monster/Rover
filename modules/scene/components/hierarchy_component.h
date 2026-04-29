#pragma once

#include "modules/scene/world.h"

#include <vector>

namespace rover
{

    // Hierarchy is expressed via two paired components: each child stores its
    // parent in `ParentComponent`, and each node optionally stores the list of
    // its direct children in `ChildrenComponent`. The serializer keeps both in
    // sync; user code should mutate via `SceneTree::set_parent(...)` once that
    // helper exists in a later phase.

    struct ParentComponent
    {
        World::EntityId parent = entt::null;
    };

    struct ChildrenComponent
    {
        std::vector<World::EntityId> children;
    };

} // namespace rover
