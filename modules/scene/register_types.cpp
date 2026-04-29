#include "modules/scene/register_types.h"

#include "core/log/log.h"

#include <memory>

namespace rover
{

    namespace
    {

        // Heap-allocated so we can fully tear down between registrations (mostly
        // useful for tests that want a clean slate).
        std::unique_ptr<World> g_default_world;

    } // namespace

    World& default_world()
    {
        if (!g_default_world)
        {
            g_default_world = std::make_unique<World>();
        }
        return *g_default_world;
    }

    void register_scene_types()
    {
        if (!g_default_world)
        {
            g_default_world = std::make_unique<World>();
        }
        ROVER_LOG_INFO("scene module registered (entities={}) ", g_default_world->entity_count());
    }

    void unregister_scene_types()
    {
        if (g_default_world)
        {
            g_default_world->clear();
            g_default_world.reset();
        }
    }

} // namespace rover
