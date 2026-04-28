message(STATUS "[Rover] Configuring modules/ui...")

rover_glob_sources(_ui_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_module_ui
    SOURCES ${_ui_sources}
    PUBLIC_DEPS
        Rover::Core
        Rover::Services
    FOLDER "Rover/Modules"
)
