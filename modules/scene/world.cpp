#include "modules/scene/world.h"

namespace rover
{

    World::EntityId World::create_entity()
    {
        return registry_.create();
    }

    void World::destroy_entity(EntityId id)
    {
        if (registry_.valid(id))
        {
            registry_.destroy(id);
        }
    }

} // namespace rover
