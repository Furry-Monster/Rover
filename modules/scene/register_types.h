#pragma once

#include "modules/scene/world.h"

namespace rover
{

    void register_scene_types();
    void unregister_scene_types();

    // ---------------------------------------------------------------------------
    // Process-wide default World. Populated by `register_scene_types()` and
    // torn down by `unregister_scene_types()`. Owners that need their own
    // isolated World (tests, editor multi-scene support) should construct one
    // directly rather than rely on this accessor.
    // ---------------------------------------------------------------------------
    World& default_world();

} // namespace rover
