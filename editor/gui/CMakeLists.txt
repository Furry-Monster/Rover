message(STATUS "[Rover] Configuring editor/gui...")

rover_glob_sources(_gui_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_editor_gui
    SOURCES ${_gui_sources}
    PUBLIC_DEPS
        Rover::Core
        Rover::Services
        Rover::Modules
        imgui
    FOLDER "Rover/Editor"
)
