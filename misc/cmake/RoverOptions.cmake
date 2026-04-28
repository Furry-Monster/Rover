# RoverOptions.cmake
# Feature toggles for the Rover engine. All options should be defined here.

include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# Platform auto-detection
# ---------------------------------------------------------------------------
if (NOT DEFINED ROVER_PLATFORM)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(ROVER_PLATFORM "linux")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(ROVER_PLATFORM "windows")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(ROVER_PLATFORM "mac")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
        set(ROVER_PLATFORM "android")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(ROVER_PLATFORM "ios")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        set(ROVER_PLATFORM "web")
    else ()
        message(FATAL_ERROR "[Rover] Unsupported platform: ${CMAKE_SYSTEM_NAME}")
    endif ()
endif ()
set(ROVER_PLATFORM "${ROVER_PLATFORM}" CACHE STRING "Target platform" FORCE)
set_property(CACHE ROVER_PLATFORM PROPERTY STRINGS
    linux windows mac android ios web)

message(STATUS "[Rover] Platform: ${ROVER_PLATFORM}")

# ---------------------------------------------------------------------------
# Driver toggles
# ---------------------------------------------------------------------------
option(ROVER_VULKAN  "Build Vulkan graphics driver" ON)
option(ROVER_D3D12   "Build D3D12 graphics driver"  OFF)
option(ROVER_METAL   "Build Metal graphics driver"  OFF)

# ---------------------------------------------------------------------------
# Top-level component toggles
# ---------------------------------------------------------------------------
option(ROVER_EDITOR  "Build editor (gui + cli)" ON)
option(ROVER_TESTS   "Build test suite"          ON)

# ---------------------------------------------------------------------------
# Module toggles (default ON; user can disable individually)
# ---------------------------------------------------------------------------
option(ROVER_MODULE_SCENE         "Build scene module"         ON)
option(ROVER_MODULE_ANIMATION     "Build animation module"     ON)
option(ROVER_MODULE_PARTICLE      "Build particle module"      ON)
option(ROVER_MODULE_UI            "Build ui module"            ON)
option(ROVER_MODULE_AI            "Build ai module"            ON)
option(ROVER_MODULE_SERIALIZATION "Build serialization module" ON)

# ---------------------------------------------------------------------------
# Diagnostics
# ---------------------------------------------------------------------------
option(ROVER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ROVER_ENABLE_ASAN        "Enable AddressSanitizer (Debug only)" OFF)
option(ROVER_ENABLE_UBSAN       "Enable UndefinedBehaviorSanitizer (Debug only)" OFF)
option(ROVER_ENABLE_TSAN        "Enable ThreadSanitizer (Debug only)"   OFF)
