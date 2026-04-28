message(STATUS "[Rover] Configuring main...")

# ---------------------------------------------------------------------------
# rover -- executable composition root.
#
# Links the abstract layers (core, services, modules) together with the
# selected concrete drivers and platform implementations. This is the only
# place where dependency inversion is resolved into concrete bindings.
# ---------------------------------------------------------------------------
add_executable(rover main.cpp)

target_link_libraries(rover
    PRIVATE
        Rover::CompileFlags
        Rover::Core
        Rover::Services
        Rover::Modules
        Rover::Drivers
        Rover::Platform
)

if (ROVER_EDITOR)
    target_link_libraries(rover PRIVATE Rover::Editor)
    target_compile_definitions(rover PRIVATE ROVER_EDITOR_BUILD=1)
endif ()

target_include_directories(rover
    PRIVATE
        "${ROVER_ROOT_DIR}"
        "${ROVER_GENERATED_DIR}"
)

set_target_properties(rover PROPERTIES
    FOLDER "Rover"
    OUTPUT_NAME "rover"
)

if (TARGET rover_copy_compile_commands)
    add_dependencies(rover_copy_compile_commands rover)
endif ()
