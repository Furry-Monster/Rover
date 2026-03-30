#include "core/log/logger.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("Logger: init and shutdown without crash")
{
    rover_log_init();

    LOG_DEBUG("Debug message: {}", 42);
    LOG_INFO("Info message: {}", "hello");
    LOG_WARN("Warning message");
    LOG_ERROR("Error message: {} + {} = {}", 1, 2, 3);

    rover_log_shutdown();
}

TEST_CASE("Logger: re-init after shutdown")
{
    rover_log_init();
    LOG_INFO("First session");
    rover_log_shutdown();

    rover_log_init();
    LOG_INFO("Second session");
    rover_log_shutdown();
}
