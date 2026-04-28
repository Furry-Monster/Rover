message(STATUS "[Rover] Configuring services...")

# ---------------------------------------------------------------------------
# rover_services -- Layer 3 aggregate of heavy singleton subsystems.
#
# Services consume the abstract interfaces declared in core; they do NOT
# depend on a specific driver. main/ wires concrete drivers into the
# services at startup via register_*_types().
# ---------------------------------------------------------------------------
rover_glob_sources(_services_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_services
    SOURCES ${_services_sources}
    PUBLIC_DEPS
        Rover::Core
    FOLDER "Rover/Services"
)

add_library(Rover::Services ALIAS rover_services)
