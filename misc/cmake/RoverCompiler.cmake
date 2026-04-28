# RoverCompiler.cmake
# Compiler flags, warnings, and sanitizer wiring for engine targets.
# Apply to engine targets only via rover_apply_compile_options(<target>).

include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# rover_compile_flags  INTERFACE library carrying common engine flags.
# Engine targets link against it PRIVATE; vendor code is unaffected.
# ---------------------------------------------------------------------------
add_library(rover_compile_flags INTERFACE)
add_library(Rover::CompileFlags ALIAS rover_compile_flags)

target_compile_features(rover_compile_flags INTERFACE cxx_std_20)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(rover_compile_flags INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wno-unknown-pragmas
        -pipe
        -fno-omit-frame-pointer
    )
    if (ROVER_WARNINGS_AS_ERRORS)
        target_compile_options(rover_compile_flags INTERFACE -Werror)
    endif ()
elseif (MSVC)
    target_compile_options(rover_compile_flags INTERFACE
        /W4
        /permissive-
        /Zc:__cplusplus
        /Zc:preprocessor
        /utf-8
        /MP
    )
    if (ROVER_WARNINGS_AS_ERRORS)
        target_compile_options(rover_compile_flags INTERFACE /WX)
    endif ()
endif ()

# ---------------------------------------------------------------------------
# Sanitizers (Debug only; mutually exclusive between TSan and ASan/UBSan).
# ---------------------------------------------------------------------------
if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND
    (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))

    if (ROVER_ENABLE_TSAN AND (ROVER_ENABLE_ASAN OR ROVER_ENABLE_UBSAN))
        message(FATAL_ERROR
            "[Rover] TSan cannot be combined with ASan/UBSan")
    endif ()

    set(_san_flags "")
    if (ROVER_ENABLE_ASAN)
        list(APPEND _san_flags -fsanitize=address)
    endif ()
    if (ROVER_ENABLE_UBSAN)
        list(APPEND _san_flags -fsanitize=undefined)
    endif ()
    if (ROVER_ENABLE_TSAN)
        list(APPEND _san_flags -fsanitize=thread)
    endif ()

    if (_san_flags)
        target_compile_options(rover_compile_flags INTERFACE ${_san_flags})
        target_link_options   (rover_compile_flags INTERFACE ${_san_flags})
        message(STATUS "[Rover] Sanitizers: ${_san_flags}")
    endif ()
endif ()

# ---------------------------------------------------------------------------
# Per-platform compile definitions (engine-wide).
# ---------------------------------------------------------------------------
if (ROVER_PLATFORM STREQUAL "linux")
    target_compile_definitions(rover_compile_flags INTERFACE ROVER_PLATFORM_LINUX=1)
elseif (ROVER_PLATFORM STREQUAL "windows")
    target_compile_definitions(rover_compile_flags INTERFACE
        ROVER_PLATFORM_WINDOWS=1
        NOMINMAX
        WIN32_LEAN_AND_MEAN)
elseif (ROVER_PLATFORM STREQUAL "mac")
    target_compile_definitions(rover_compile_flags INTERFACE ROVER_PLATFORM_MAC=1)
elseif (ROVER_PLATFORM STREQUAL "android")
    target_compile_definitions(rover_compile_flags INTERFACE ROVER_PLATFORM_ANDROID=1)
elseif (ROVER_PLATFORM STREQUAL "ios")
    target_compile_definitions(rover_compile_flags INTERFACE ROVER_PLATFORM_IOS=1)
elseif (ROVER_PLATFORM STREQUAL "web")
    target_compile_definitions(rover_compile_flags INTERFACE ROVER_PLATFORM_WEB=1)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(rover_compile_flags INTERFACE ROVER_DEBUG=1)
else ()
    target_compile_definitions(rover_compile_flags INTERFACE ROVER_RELEASE=1)
endif ()
