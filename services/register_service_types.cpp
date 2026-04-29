#include "services/register_service_types.h"

#include "core/log/log.h"
#include "services/graphics/graphics_service.h"

namespace rover
{

    void register_service_types()
    {
        // GraphicsService is a Meyers singleton; touching it constructs the
        // instance. The actual GraphicsDevice pointer is wired later by main/
        // once the driver is initialized.
        (void)GraphicsService::get();
        ROVER_LOG_INFO("Service singletons registered (graphics)");
    }

    void unregister_service_types()
    {
        GraphicsService::get().unbind_device();
    }

} // namespace rover
