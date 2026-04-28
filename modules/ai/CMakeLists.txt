message(STATUS "[Rover] Configuring modules/ai...")

rover_glob_sources(_ai_sources "${CMAKE_CURRENT_SOURCE_DIR}")

rover_add_library(rover_module_ai
    SOURCES ${_ai_sources}
    PUBLIC_DEPS
        Rover::Core
        Rover::Services
    FOLDER "Rover/Modules"
)
