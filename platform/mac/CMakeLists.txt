message(STATUS "[Rover] Configuring platform/mac...")

rover_glob_sources(_mac_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_platform_mac
    SOURCES ${_mac_sources}
    PUBLIC_DEPS
        Rover::Core
        SDL3::SDL3
        rover_platform_libs
    FOLDER "Rover/Platform"
)
