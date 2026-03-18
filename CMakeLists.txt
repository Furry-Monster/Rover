cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(RoverEngine
        VERSION 0.1.0
        LANGUAGES CXX C
        DESCRIPTION "Vulkan + SDL3 game engine for Windows and Linux"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/misc/cmake")
include(RoverOptions)
include(RoverCompiler)
include(RoverUtils)

# Build type: validate or set default for single-config generators.
if (NOT CMAKE_CONFIGURATION_TYPES)
    if (NOT CMAKE_BUILD_TYPE)
        message(STATUS "[Rover] CMAKE_BUILD_TYPE not set, defaulting to Debug")
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug | Release)" FORCE)
    endif ()
    if (NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
        message(FATAL_ERROR "[Rover] CMAKE_BUILD_TYPE must be Debug or Release, got: '${CMAKE_BUILD_TYPE}'")
    endif ()
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ROVER_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
string(TOLOWER "${CMAKE_BUILD_TYPE}" _build_type_lower)
set(ROVER_BIN_DIR "${ROVER_ROOT_DIR}/bin/${_build_type_lower}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ROVER_BIN_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ROVER_BIN_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${ROVER_BIN_DIR}/lib")

foreach (_config Debug Release)
    string(TOUPPER ${_config} _config_upper)
    string(TOLOWER ${_config} _config_lower)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config_upper} "${ROVER_ROOT_DIR}/bin/${_config_lower}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_config_upper} "${ROVER_ROOT_DIR}/bin/${_config_lower}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_config_upper} "${ROVER_ROOT_DIR}/bin/${_config_lower}/lib")
endforeach ()

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/misc/cmake/RoverVersion.h.in"
        "${CMAKE_BINARY_DIR}/generated/rover_version.h"
        @ONLY
)

# Keep compile_commands.json at project root so clangd always finds it.
if (CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(copy_compile_commands ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${ROVER_ROOT_DIR}/compile_commands.json"
        COMMENT "[Rover] Refreshing compile_commands.json"
        VERBATIM
    )
endif ()

add_subdirectory(core)
add_subdirectory(vendor)
add_subdirectory(scene)
add_subdirectory(platform)
add_subdirectory(servers)
add_subdirectory(main)
add_subdirectory(tests)
