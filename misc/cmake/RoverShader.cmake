# Compile Slang/HLSL to SPIR-V, then embed deterministic C++ headers.
include_guard(GLOBAL)

if(NOT TARGET Rover::SlangCompiler)
    message(FATAL_ERROR "[Rover] The vendored Slang compiler target is required")
endif ()
find_program(ROVER_SHADER_PYTHON NAMES python3 python REQUIRED)

function(rover_add_shader OUT_VAR)
    cmake_parse_arguments(ARG "" "SHADER;STAGE;ENTRY;TARGET;VAR;OUTPUT_DIR" "" ${ARGN})
    if(NOT ARG_SHADER OR NOT ARG_STAGE OR NOT ARG_VAR OR NOT ARG_OUTPUT_DIR)
        message(FATAL_ERROR "[Rover] rover_add_shader requires SHADER, STAGE, VAR and OUTPUT_DIR")
    endif ()
    if(NOT ARG_SHADER MATCHES "\\.(slang|hlsl)$")
        message(FATAL_ERROR "[Rover] Shader source must be .slang or .hlsl: ${ARG_SHADER}")
    endif ()
    if(ARG_TARGET AND NOT ARG_TARGET STREQUAL "spirv")
        message(FATAL_ERROR "[Rover] The current shader build supports TARGET spirv")
    endif ()
    if(NOT ARG_ENTRY)
        set(ARG_ENTRY main)
    endif ()
    set(_stage "${ARG_STAGE}")
    if(_stage STREQUAL "vert")
        set(_stage vertex)
    elseif(_stage STREQUAL "frag")
        set(_stage fragment)
    elseif(_stage STREQUAL "comp")
        set(_stage compute)
    endif ()
    string(REGEX REPLACE "_spv$" "" _name "${ARG_VAR}")
    set(_spv "${ARG_OUTPUT_DIR}/${_name}.spv")
    set(_header "${_spv}.h")
    set(_depfile "${_spv}.d")
    set(_embed "${ROVER_ROOT_DIR}/misc/scripts/spirv_to_header.py")
    file(MAKE_DIRECTORY "${ARG_OUTPUT_DIR}")
    add_custom_command(
        OUTPUT "${_spv}" "${_header}"
        COMMAND "$<TARGET_FILE:slangc>" "${ARG_SHADER}"
                -target spirv -profile spirv_1_6 -stage "${_stage}" -entry "${ARG_ENTRY}"
                -matrix-layout-column-major -emit-spirv-directly -O0
                -depfile "${_depfile}" -o "${_spv}"
        COMMAND "${ROVER_SHADER_PYTHON}" "${_embed}" "${_spv}" "${_header}" "${ARG_VAR}"
        DEPENDS "${ARG_SHADER}" "${_embed}" slangc
        DEPFILE "${_depfile}"
        COMMENT "[Rover] Slang ${_name}: ${ARG_ENTRY} -> SPIR-V"
        VERBATIM
    )
    set(${OUT_VAR} "${${OUT_VAR}};${_header}" PARENT_SCOPE)
endfunction()
