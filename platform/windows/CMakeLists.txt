message(STATUS "[Rover] Configuring platform/windows...")

rover_glob_sources(_windows_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_platform_windows
    SOURCES ${_windows_sources}
    PUBLIC_DEPS
        Rover::Core
        SDL3::SDL3
        rover_platform_libs
    FOLDER "Rover/Platform"
)
