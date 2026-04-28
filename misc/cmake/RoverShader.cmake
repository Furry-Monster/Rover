# RoverShader.cmake
# Compile GLSL shaders to SPIR-V and embed them as C headers.

include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# Locate a GLSL → SPIR-V compiler.
# Prefer glslangValidator (ships with the Vulkan SDK) which supports the
# --vn flag to emit a C header with a named uint32_t array directly.
# ---------------------------------------------------------------------------
find_program(GLSLANG_VALIDATOR
    NAMES glslangValidator
    HINTS
        ENV VULKAN_SDK
        ENV VULKAN_SDK_PATH
    PATH_SUFFIXES bin
)

if (NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR
        "[Rover] glslangValidator not found. Install the Vulkan SDK or set VULKAN_SDK.")
endif ()

message(STATUS "[Rover] Shader compiler: ${GLSLANG_VALIDATOR}")

# ---------------------------------------------------------------------------
# rover_add_shader(<output_var>
#     SHADER  <path/to/foo.vert.glsl>
#     STAGE   <vert|frag|comp|geom|tesc|tese>
#     VAR     <c_array_name>
#     OUTPUT_DIR <generated_dir>)
#
# Adds a build rule that compiles SHADER -> Vulkan 1.3 SPIR-V and emits a C
# header at <OUTPUT_DIR>/<basename>.spv.h declaring:
#     static const uint32_t <VAR>[] = { ... };
#
# Appends the generated header path to <output_var> for use in target SOURCES.
# ---------------------------------------------------------------------------
function(rover_add_shader OUT_VAR)
    cmake_parse_arguments(ARG
        ""
        "SHADER;STAGE;VAR;OUTPUT_DIR"
        ""
        ${ARGN})

    if (NOT ARG_SHADER OR NOT ARG_STAGE OR NOT ARG_VAR OR NOT ARG_OUTPUT_DIR)
        message(FATAL_ERROR "[Rover] rover_add_shader: SHADER, STAGE, VAR, OUTPUT_DIR are required")
    endif ()

    get_filename_component(_shader_name "${ARG_SHADER}" NAME_WE)
    set(_header "${ARG_OUTPUT_DIR}/${_shader_name}_${ARG_STAGE}.spv.h")

    file(MAKE_DIRECTORY "${ARG_OUTPUT_DIR}")

    add_custom_command(
        OUTPUT  "${_header}"
        COMMAND "${GLSLANG_VALIDATOR}"
                -V                          # produce SPIR-V
                --target-env vulkan1.3
                -S ${ARG_STAGE}
                --vn ${ARG_VAR}
                -o "${_header}"
                "${ARG_SHADER}"
        DEPENDS "${ARG_SHADER}"
        COMMENT "[Rover] Compiling shader ${_shader_name}.${ARG_STAGE} -> ${_header}"
        VERBATIM
    )

    set(${OUT_VAR} "${${OUT_VAR}};${_header}" PARENT_SCOPE)
endfunction()
