#include "platform/linux/register_types.h"

namespace rover {

void register_linux_platform() {
    // TODO: initialize SDL subsystems, register Linux-specific OS services
    // (filesystem, threading hooks, time source) with core.
}

void unregister_linux_platform() {
    // TODO: shutdown SDL and release Linux platform resources.
}

} // namespace rover
