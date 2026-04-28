message(STATUS "[Rover] Configuring editor...")

# ---------------------------------------------------------------------------
# editor/gui  -- ImGui-based visual editor (humans)
# editor/cli  -- structured command line (AI / scripting)
# Both consume the same module + service layer; they share the editor
# bootstrap defined here.
# ---------------------------------------------------------------------------
add_subdirectory(gui)
add_subdirectory(cli)

rover_add_library(rover_editor
    SOURCES
        register_editor_types.h
        register_editor_types.cpp
    PUBLIC_DEPS
        Rover::Core
        Rover::Services
        Rover::Modules
        rover_editor_gui
        rover_editor_cli
    FOLDER "Rover/Editor"
)

add_library(Rover::Editor ALIAS rover_editor)
