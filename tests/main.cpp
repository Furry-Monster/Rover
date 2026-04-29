#define DOCTEST_CONFIG_IMPLEMENT
#include "core/log/log.h"

#include <doctest/doctest.h>

int
main(int argc, char** argv)
{
    // Initialize the engine's core logger before doctest runs any test cases.
    // Several engine subsystems (frame graph, vulkan driver) emit warnings
    // through ROVER_LOG_*, which would crash if the logger were unconfigured.
    rover::Log::init();

    doctest::Context ctx(argc, argv);
    const int        rc = ctx.run();
    rover::Log::shutdown();
    return rc;
}
