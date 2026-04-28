message(STATUS "[Rover] Configuring main...")

include(RoverShader)

# ---------------------------------------------------------------------------
# Compile demo shaders to SPIR-V headers consumed by main.cpp.
# Generated headers land under ${CMAKE_CURRENT_BINARY_DIR}/shaders/.
# ---------------------------------------------------------------------------
set(_main_shader_headers "")
set(_shader_out_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")

rover_add_shader(_main_shader_headers
    SHADER     "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert.glsl"
    STAGE      vert
    VAR        triangle_vert_spv
    OUTPUT_DIR "${_shader_out_dir}")

rover_add_shader(_main_shader_headers
    SHADER     "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag.glsl"
    STAGE      frag
    VAR        triangle_frag_spv
    OUTPUT_DIR "${_shader_out_dir}")

# ---------------------------------------------------------------------------
# rover -- executable composition root.
#
# Links the abstract layers (core, services, modules) together with the
# selected concrete drivers and platform implementations. This is the only
# place where dependency inversion is resolved into concrete bindings.
# ---------------------------------------------------------------------------
add_executable(rover main.cpp ${_main_shader_headers})

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
        "${CMAKE_CURRENT_BINARY_DIR}"   # shader headers: shaders/triangle_vert.spv.h
)

set_target_properties(rover PROPERTIES
    FOLDER "Rover"
    OUTPUT_NAME "rover"
)

if (TARGET rover_copy_compile_commands)
    add_dependencies(rover_copy_compile_commands rover)
endif ()
