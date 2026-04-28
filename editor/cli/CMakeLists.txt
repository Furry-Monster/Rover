message(STATUS "[Rover] Configuring editor/cli...")

rover_glob_sources(_cli_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_editor_cli
    SOURCES ${_cli_sources}
    PUBLIC_DEPS
        Rover::Core
        Rover::Services
        Rover::Modules
    FOLDER "Rover/Editor"
)
