message(STATUS "[Rover] Configuring platform/android...")

rover_glob_sources(_android_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_platform_android
    SOURCES ${_android_sources}
    PUBLIC_DEPS
        Rover::Core
        SDL3::SDL3
        rover_platform_libs
    FOLDER "Rover/Platform"
)
