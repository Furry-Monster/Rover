#include "platform/linux/register_types.h"

#include "core/log/log.h"
#include "platform/linux/linux_platform.h"

namespace rover {

void register_linux_platform() {
    if (!LinuxPlatform::get().init()) {
        ROVER_LOG_FATAL("Failed to initialize Linux platform");
        return;
    }
    ROVER_LOG_INFO("Linux platform initialized");
}

void unregister_linux_platform() {
    LinuxPlatform::get().shutdown();
    ROVER_LOG_INFO("Linux platform shut down");
}

} // namespace rover
