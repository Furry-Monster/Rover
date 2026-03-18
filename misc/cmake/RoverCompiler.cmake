add_library(rover_compiler_options INTERFACE)
add_library(Rover::CompilerOptions ALIAS rover_compiler_options)

if (MSVC)
    target_compile_options(rover_compiler_options INTERFACE
        /W4
        /utf-8
        /permissive-
        /Zc:__cplusplus
        /MP                  # parallel file compilation
        /Zc:preprocessor     # conformant preprocessor (required for some C++20 features)
    )
    target_compile_options(rover_compiler_options INTERFACE
        "$<$<CONFIG:Debug>:/Od;/RTC1>"
        "$<$<CONFIG:Release>:/O2;/Oi;/GL>"
    )
    target_link_options(rover_compiler_options INTERFACE
        "$<$<CONFIG:Release>:/LTCG>"
    )
else ()
    target_compile_options(rover_compiler_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wno-unknown-pragmas
        -pipe                # use pipes instead of temp files, faster compilation
    )
    target_compile_options(rover_compiler_options INTERFACE
        "$<$<CONFIG:Debug>:-fno-omit-frame-pointer>"
        "$<$<CONFIG:Release>:-O3>"
    )
endif ()

# LTO: only effective in Release, single-config generators.
if (ROVER_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg)
    if (_ipo_ok)
        set_property(TARGET rover_compiler_options PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else ()
        message(WARNING "[Rover] LTO not supported: ${_ipo_msg}")
    endif ()
endif ()

# AddressSanitizer (ROVER_ENABLE_ASAN option declared in RoverOptions.cmake).
add_library(rover_sanitizer_options INTERFACE)
add_library(Rover::SanitizerOptions ALIAS rover_sanitizer_options)

if (ROVER_ENABLE_ASAN)
    if (MSVC)
        target_compile_options(rover_sanitizer_options INTERFACE /fsanitize=address)
    else ()
        target_compile_options(rover_sanitizer_options INTERFACE
            -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(rover_sanitizer_options INTERFACE -fsanitize=address)
    endif ()
endif ()
