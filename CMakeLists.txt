cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(RoverEngine
        VERSION 0.1.0
        LANGUAGES CXX C
        DESCRIPTION "Rover Game Engine"
)

# ---------------------------------------------------------------------------
# Build type: validate or set default for single-config generators.
# ---------------------------------------------------------------------------
if (NOT CMAKE_CONFIGURATION_TYPES)
    if (NOT CMAKE_BUILD_TYPE)
        message(STATUS "[Rover] CMAKE_BUILD_TYPE not set, defaulting to Debug")
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
            "Build type (Debug | Release)" FORCE)
    endif ()
    if (NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
        message(FATAL_ERROR
            "[Rover] CMAKE_BUILD_TYPE must be Debug or Release, got: '${CMAKE_BUILD_TYPE}'")
    endif ()
endif ()

# ---------------------------------------------------------------------------
# Language standard and global toolchain settings.
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Module path and engine helpers.
# ---------------------------------------------------------------------------
set(ROVER_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${ROVER_ROOT_DIR}/misc/cmake")

include(RoverOptions)
include(RoverCompiler)
include(RoverUtils)

# ---------------------------------------------------------------------------
# Output directories: bin/<config>/  and  bin/<config>/lib/
# ---------------------------------------------------------------------------
string(TOLOWER "${CMAKE_BUILD_TYPE}" _build_type_lower)
set(ROVER_BIN_DIR "${ROVER_ROOT_DIR}/bin/${_build_type_lower}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ROVER_BIN_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ROVER_BIN_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${ROVER_BIN_DIR}/lib")

foreach (_config Debug Release)
    string(TOUPPER ${_config} _config_upper)
    string(TOLOWER ${_config} _config_lower)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config_upper}
        "${ROVER_ROOT_DIR}/bin/${_config_lower}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_config_upper}
        "${ROVER_ROOT_DIR}/bin/${_config_lower}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_config_upper}
        "${ROVER_ROOT_DIR}/bin/${_config_lower}/lib")
endforeach ()

# ---------------------------------------------------------------------------
# Generated headers (version, module registration, etc.)
# ---------------------------------------------------------------------------
set(ROVER_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${ROVER_GENERATED_DIR}")

configure_file(
    "${ROVER_ROOT_DIR}/misc/cmake/RoverVersion.h.in"
    "${ROVER_GENERATED_DIR}/rover_version.h"
    @ONLY
)

# ---------------------------------------------------------------------------
# Keep compile_commands.json at project root so clangd always finds it.
# ---------------------------------------------------------------------------
if (CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(rover_copy_compile_commands ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${ROVER_ROOT_DIR}/compile_commands.json"
        COMMENT "[Rover] Refreshing compile_commands.json"
        VERBATIM
    )
endif ()

# ---------------------------------------------------------------------------
# Subdirectory build order. Matches Layer 0 -> Layer 5 + main.
#
#   vendor    Layer 0 -- third-party
#   core      Layer 1 -- foundation + abstract interfaces
#   drivers   Layer 2 -- backend implementations
#   platform  Layer 2 -- OS-specific entry + windowing
#   services  Layer 3 -- heavy singleton systems
#   modules   Layer 4 -- pluggable optional features
#   editor    Layer 5 -- gui + cli (optional)
#   tests     -- doctest suite (optional)
#   main      -- composition root, executable
# ---------------------------------------------------------------------------
add_subdirectory(vendor)
add_subdirectory(core)
add_subdirectory(drivers)
add_subdirectory(platform)
add_subdirectory(services)
add_subdirectory(modules)

if (ROVER_EDITOR)
    add_subdirectory(editor)
endif ()

if (ROVER_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

add_subdirectory(main)

# ---------------------------------------------------------------------------
# Configuration summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "[Rover] === Configuration Summary ===")
message(STATUS "[Rover] Build type    : ${CMAKE_BUILD_TYPE}")
message(STATUS "[Rover] Platform      : ${ROVER_PLATFORM}")
message(STATUS "[Rover] Vulkan driver : ${ROVER_VULKAN}")
message(STATUS "[Rover] D3D12 driver  : ${ROVER_D3D12}")
message(STATUS "[Rover] Metal driver  : ${ROVER_METAL}")
message(STATUS "[Rover] Editor        : ${ROVER_EDITOR}")
message(STATUS "[Rover] Tests         : ${ROVER_TESTS}")
message(STATUS "[Rover] =============================")
message(STATUS "")
