cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(RoverEngine
        VERSION 0.1.0
        LANGUAGES CXX C
        DESCRIPTION "Vulkan + SDL3 game engine for Windows and Linux"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/misc/cmake")
include(RoverOptions)
include(RoverCompiler)
include(RoverUtils)

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

set(_rover_resource_dirs "")
if (EXISTS "${ROVER_ROOT_DIR}/assets")
    list(APPEND _rover_resource_dirs "${ROVER_ROOT_DIR}/assets")
endif ()
if (EXISTS "${ROVER_ROOT_DIR}/shaders")
    list(APPEND _rover_resource_dirs "${ROVER_ROOT_DIR}/shaders")
endif ()
if (_rover_resource_dirs)
    rover_copy_resources(rover_copy_resources
            DIRECTORIES ${_rover_resource_dirs}
            DESTINATION "${ROVER_BIN_DIR}"
    )
endif ()

if (EXISTS "${ROVER_ROOT_DIR}/core/CMakeLists.txt")
    add_subdirectory(core)
endif ()

if (EXISTS "${ROVER_ROOT_DIR}/vendor/CMakeLists.txt")
    add_subdirectory(vendor)
endif ()

if (EXISTS "${ROVER_ROOT_DIR}/scene/CMakeLists.txt")
    add_subdirectory(scene)
endif ()

if (EXISTS "${ROVER_ROOT_DIR}/platform/CMakeLists.txt")
    add_subdirectory(platform)
endif ()

if (EXISTS "${ROVER_ROOT_DIR}/servers/CMakeLists.txt")
    add_subdirectory(servers)
endif ()

if (EXISTS "${ROVER_ROOT_DIR}/main/CMakeLists.txt")
    add_subdirectory(main)
endif ()

if (ROVER_BUILD_TESTS AND EXISTS "${ROVER_ROOT_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif ()
