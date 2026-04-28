message(STATUS "[Rover] Configuring modules...")

# ---------------------------------------------------------------------------
# Discover enabled modules. A module is built when both:
#   - its directory contains a CMakeLists.txt
#   - the option ROVER_MODULE_<NAME> is ON (defined in RoverOptions.cmake)
# Each module produces rover_module_<name> static library.
# ---------------------------------------------------------------------------
rover_collect_modules(ROVER_ENABLED_MODULES "${CMAKE_CURRENT_SOURCE_DIR}")

set(_module_targets "")
foreach (_module IN LISTS ROVER_ENABLED_MODULES)
    set(_module_cmake "${CMAKE_CURRENT_SOURCE_DIR}/${_module}/CMakeLists.txt")
    if (EXISTS "${_module_cmake}")
        add_subdirectory(${_module})
        list(APPEND _module_targets "rover_module_${_module}")
        message(STATUS "[Rover] Module enabled: ${_module}")
    else ()
        message(STATUS "[Rover] Module skipped (no CMakeLists.txt): ${_module}")
    endif ()
endforeach ()

# ---------------------------------------------------------------------------
# Generate the aggregate registration source from the enabled module list.
# ---------------------------------------------------------------------------
set(_modules_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_modules_gen_dir}")
set(_modules_gen_file "${_modules_gen_dir}/register_module_types.gen.cpp")

set(_module_names "")
foreach (_t IN LISTS _module_targets)
    string(REGEX REPLACE "^rover_module_" "" _name "${_t}")
    list(APPEND _module_names "${_name}")
endforeach ()
rover_generate_module_registration("${_modules_gen_file}" ${_module_names})

# ---------------------------------------------------------------------------
# rover_modules -- aggregate library that exposes register_module_types().
# ---------------------------------------------------------------------------
rover_add_library(rover_modules
    SOURCES
        register_module_types.h
        "${_modules_gen_file}"
    PUBLIC_DEPS
        Rover::Core
        Rover::Services
        ${_module_targets}
    FOLDER "Rover/Modules"
)

add_library(Rover::Modules ALIAS rover_modules)
