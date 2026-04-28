message(STATUS "[Rover] Configuring platform (${ROVER_PLATFORM})...")

# ---------------------------------------------------------------------------
# Only the selected platform's subdirectory is built. Each platform produces
# rover_platform_<name> and exposes a register_platform_apis() entry point.
# ---------------------------------------------------------------------------
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${ROVER_PLATFORM}/CMakeLists.txt")
    message(FATAL_ERROR
        "[Rover] platform/${ROVER_PLATFORM}/CMakeLists.txt not found")
endif ()

add_subdirectory(${ROVER_PLATFORM})

# ---------------------------------------------------------------------------
# rover_platform -- alias façade so dependents (services, main) link a
# stable name regardless of which OS was selected.
# ---------------------------------------------------------------------------
rover_add_library(rover_platform
    SOURCES
        register_platform_apis.cpp
        register_platform_apis.h
    PUBLIC_DEPS
        Rover::Core
        rover_platform_${ROVER_PLATFORM}
    FOLDER "Rover/Platform"
)

add_library(Rover::Platform ALIAS rover_platform)
