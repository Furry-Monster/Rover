message(STATUS "[Rover] Configuring modules/scene...")

rover_glob_sources(_scene_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_module_scene
    SOURCES ${_scene_sources}
    PUBLIC_DEPS
        Rover::Core
        Rover::Services
    FOLDER "Rover/Modules"
)
