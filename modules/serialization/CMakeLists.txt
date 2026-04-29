message(STATUS "[Rover] Configuring modules/serialization...")

rover_glob_sources(_serialization_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_module_serialization
    SOURCES ${_serialization_sources}
    PUBLIC_DEPS
        Rover::Core
        Rover::Services
        rover_module_scene
    FOLDER "Rover/Modules"
)
