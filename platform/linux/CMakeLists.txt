message(STATUS "[Rover] Configuring platform/linux...")

# Re-locate Vulkan in this scope so the Vulkan::Headers IMPORTED target
# becomes visible here (find_package targets are directory-scoped). This
# is idempotent against the find_package call in vendor/CMakeLists.txt.
find_package(Vulkan REQUIRED)

rover_glob_sources(_linux_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_platform_linux
    SOURCES ${_linux_sources}
    PUBLIC_DEPS
        Rover::Core
        SDL3::SDL3
        Vulkan::Headers
        rover_platform_libs
    FOLDER "Rover/Platform"
)
