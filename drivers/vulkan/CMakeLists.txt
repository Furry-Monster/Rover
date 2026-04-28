message(STATUS "[Rover] Configuring drivers/vulkan...")

rover_glob_sources(_vulkan_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_driver_vulkan
    SOURCES ${_vulkan_sources}
    PUBLIC_DEPS
        Rover::Core
        Rover::Vulkan
    FOLDER "Rover/Drivers"
)

target_compile_definitions(rover_driver_vulkan PUBLIC ROVER_DRIVER_VULKAN=1)
