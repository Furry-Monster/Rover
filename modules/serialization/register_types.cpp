#include "modules/serialization/register_types.h"

#include "core/log/log.h"
#include "modules/serialization/asset_registry.h"

namespace rover
{

    void register_serialization_types()
    {
        // Touch the asset registry so it is constructed deterministically here
        // rather than lazily on first use from another subsystem.
        (void)AssetRegistry::get();
        ROVER_LOG_INFO("serialization module registered");
    }

    void unregister_serialization_types()
    {
        AssetRegistry::get().clear();
    }

} // namespace rover
