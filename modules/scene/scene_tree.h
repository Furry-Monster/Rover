#pragma once

#include "core/math/mat4.h"
#include "modules/scene/world.h"

namespace rover
{

    // ---------------------------------------------------------------------------
    // SceneTree: free-function helpers for parent/child relationships and
    // world-space transform computation.
    //
    // Phase 2 keeps the API small: link / unlink + world transform query.
    // Storing world transforms in a dedicated component (cached + dirty-flag
    // driven) is left to Phase 3 along with batched dirty propagation.
    // ---------------------------------------------------------------------------
    class SceneTree
    {
    public:
        static void set_parent(World& world, World::EntityId child, World::EntityId parent);
        static void unparent(World& world, World::EntityId child);

        // Walks up the parent chain composing local transforms. Returns identity
        // if the entity has no `TransformComponent`.
        [[nodiscard]] static Mat4 world_matrix(World& world, World::EntityId entity);
    };

} // namespace rover
