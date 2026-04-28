message(STATUS "[Rover] Configuring drivers...")

# ---------------------------------------------------------------------------
# Each driver is its own static library that depends on Rover::Core for the
# abstract interface it implements (e.g. graphics_device.h). main/ is the
# composition root that links the concrete drivers selected at build time.
# ---------------------------------------------------------------------------

set(ROVER_ENABLED_DRIVERS "" CACHE INTERNAL "List of enabled driver target names")

if (ROVER_VULKAN)
    add_subdirectory(vulkan)
    list(APPEND ROVER_ENABLED_DRIVERS rover_driver_vulkan)
endif ()

if (ROVER_D3D12 AND ROVER_PLATFORM STREQUAL "windows")
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/d3d12/CMakeLists.txt")
        add_subdirectory(d3d12)
        list(APPEND ROVER_ENABLED_DRIVERS rover_driver_d3d12)
    else ()
        message(WARNING "[Rover] ROVER_D3D12=ON but drivers/d3d12 has no CMakeLists.txt")
    endif ()
endif ()

if (ROVER_METAL AND (ROVER_PLATFORM STREQUAL "mac" OR ROVER_PLATFORM STREQUAL "ios"))
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/metal/CMakeLists.txt")
        add_subdirectory(metal)
        list(APPEND ROVER_ENABLED_DRIVERS rover_driver_metal)
    else ()
        message(WARNING "[Rover] ROVER_METAL=ON but drivers/metal has no CMakeLists.txt")
    endif ()
endif ()

set(ROVER_ENABLED_DRIVERS "${ROVER_ENABLED_DRIVERS}" CACHE INTERNAL
    "List of enabled driver target names" FORCE)

# ---------------------------------------------------------------------------
# rover_drivers -- aggregate that registers all enabled drivers and exposes
# a single dependency for main/ to link.
# ---------------------------------------------------------------------------
rover_add_library(rover_drivers
    SOURCES
        register_driver_types.cpp
        register_driver_types.h
    PUBLIC_DEPS
        Rover::Core
        ${ROVER_ENABLED_DRIVERS}
    FOLDER "Rover/Drivers"
)

add_library(Rover::Drivers ALIAS rover_drivers)
