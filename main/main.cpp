// Rover engine entry point.
//
// Initialization order follows the layer dependency direction:
//   core -> services -> drivers -> platform -> modules -> editor
// Shutdown is reverse order.

#include "rover_version.h"

#include "core/register_core_types.h"
#include "drivers/register_driver_types.h"
#include "modules/register_module_types.h"
#include "platform/register_platform_apis.h"
#include "services/register_service_types.h"

#ifdef ROVER_EDITOR_BUILD
#include "editor/register_editor_types.h"
#endif

#include <cstdio>

namespace rover {

static int run_main_loop() {
    // TODO: drive the engine main loop -- pump platform events, tick
    // services, dispatch module updates, render frame.
    return 0;
}

static int rover_main(int /*argc*/, char** /*argv*/) {
    std::printf("[Rover] %s %s\n", ROVER_VERSION_NAME, ROVER_VERSION_STRING);

    register_core_types();
    register_service_types();
    register_driver_types();
    register_platform_apis();
    register_module_types();
#ifdef ROVER_EDITOR_BUILD
    register_editor_types();
#endif

    const int exit_code = run_main_loop();

#ifdef ROVER_EDITOR_BUILD
    unregister_editor_types();
#endif
    unregister_module_types();
    unregister_platform_apis();
    unregister_driver_types();
    unregister_service_types();
    unregister_core_types();

    return exit_code;
}

} // namespace rover

int main(int argc, char** argv) {
    return rover::rover_main(argc, argv);
}
