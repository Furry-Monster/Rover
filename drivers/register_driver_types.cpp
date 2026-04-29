#include "drivers/register_driver_types.h"

#ifdef ROVER_DRIVER_VULKAN
    #include "drivers/vulkan/register_types.h"
#endif

namespace rover
{

    void register_driver_types()
    {
#ifdef ROVER_DRIVER_VULKAN
        register_vulkan_driver();
#endif
    }

    void unregister_driver_types()
    {
#ifdef ROVER_DRIVER_VULKAN
        unregister_vulkan_driver();
#endif
    }

} // namespace rover
