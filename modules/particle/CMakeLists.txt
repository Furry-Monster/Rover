message(STATUS "[Rover] Configuring modules/particle...")

rover_glob_sources(_particle_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_module_particle
    SOURCES ${_particle_sources}
    PUBLIC_DEPS
        Rover::Core
        Rover::Services
    FOLDER "Rover/Modules"
)
