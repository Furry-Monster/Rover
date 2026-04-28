message(STATUS "[Rover] Configuring platform/linux...")

rover_glob_sources(_linux_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_platform_linux
    SOURCES ${_linux_sources}
    PUBLIC_DEPS
        Rover::Core
        SDL3::SDL3
        rover_platform_libs
    FOLDER "Rover/Platform"
)
