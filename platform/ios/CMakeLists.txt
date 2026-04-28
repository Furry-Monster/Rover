message(STATUS "[Rover] Configuring platform/ios...")

rover_glob_sources(_ios_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_platform_ios
    SOURCES ${_ios_sources}
    PUBLIC_DEPS
        Rover::Core
        SDL3::SDL3
        rover_platform_libs
    FOLDER "Rover/Platform"
)
