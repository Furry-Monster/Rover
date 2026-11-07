message(STATUS "[Rover] Configuring main...")

include(RoverShader)

# ---------------------------------------------------------------------------
# Compile demo shaders to SPIR-V headers consumed by main.cpp.
# Generated headers land under ${CMAKE_CURRENT_BINARY_DIR}/shaders/.
# ---------------------------------------------------------------------------
set(_main_shader_headers "")
set(_shader_out_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")

rover_add_shader(_main_shader_headers
    SHADER     "${CMAKE_CURRENT_SOURCE_DIR}/builtin_assets/shaders/triangle.slang"
    STAGE      vert
    ENTRY      vertex_main
    VAR        triangle_vert_spv
    OUTPUT_DIR "${_shader_out_dir}")

rover_add_shader(_main_shader_headers
    SHADER     "${CMAKE_CURRENT_SOURCE_DIR}/builtin_assets/shaders/triangle.slang"
    STAGE      frag
    ENTRY      fragment_main
    VAR        triangle_frag_spv
    OUTPUT_DIR "${_shader_out_dir}")

rover_add_shader(_main_shader_headers
    SHADER     "${CMAKE_CURRENT_SOURCE_DIR}/builtin_assets/shaders/mesh.slang"
    STAGE      vert
    ENTRY      vertex_main
    VAR        mesh_vert_spv
    OUTPUT_DIR "${_shader_out_dir}")

rover_add_shader(_main_shader_headers
    SHADER     "${CMAKE_CURRENT_SOURCE_DIR}/builtin_assets/shaders/mesh.slang"
    STAGE      frag
    ENTRY      fragment_main
    VAR        mesh_frag_spv
    OUTPUT_DIR "${_shader_out_dir}")

foreach (_entry vertex_main tone_map_main present_main)
    if (_entry STREQUAL "vertex_main")
        set(_stage vert)
        set(_symbol post_process_vert_spv)
    elseif (_entry STREQUAL "tone_map_main")
        set(_stage frag)
        set(_symbol tone_map_frag_spv)
    else ()
        set(_stage frag)
        set(_symbol present_frag_spv)
    endif ()
    rover_add_shader(_main_shader_headers
        SHADER "${CMAKE_CURRENT_SOURCE_DIR}/builtin_assets/shaders/post_process.slang"
        STAGE "${_stage}" ENTRY "${_entry}" VAR "${_symbol}"
        OUTPUT_DIR "${_shader_out_dir}")
endforeach ()

# ---------------------------------------------------------------------------
# rover -- executable composition root.
#
# Links the abstract layers (core, services, modules) together with the
# selected concrete drivers and platform implementations. This is the only
# place where dependency inversion is resolved into concrete bindings.
# ---------------------------------------------------------------------------
add_executable(rover
    main.cpp
    mesh_demo.cpp
    frame_graph_demo.cpp
    subresource_demo.cpp
    ${_main_shader_headers})

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
