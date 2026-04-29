message(STATUS "[Rover] Configuring core...")

# ---------------------------------------------------------------------------
# rover_core -- Layer 1 foundation library.
#
# Glob sources from all core subsystems. Math is handwritten (see core/math/).
# ---------------------------------------------------------------------------
rover_glob_sources(_core_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_core
    SOURCES ${_core_sources}
    PUBLIC_DEPS
        spdlog
        entt
    INCLUDE_DIRS
        "${ROVER_GENERATED_DIR}"
    FOLDER "Rover/Core"
)

add_library(Rover::Core ALIAS rover_core)
