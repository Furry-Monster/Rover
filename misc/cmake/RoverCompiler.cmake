add_library(rover_compiler_options INTERFACE)
add_library(Rover::CompilerOptions ALIAS rover_compiler_options)

if(MSVC)
    target_compile_options(rover_compiler_options INTERFACE
        /W4 /utf-8 /permissive- /Zc:__cplusplus
    )
else()
    target_compile_options(rover_compiler_options INTERFACE
        -Wall -Wextra -Wpedantic
    )
endif()

add_library(rover_sanitizer_options INTERFACE)
add_library(Rover::SanitizerOptions ALIAS rover_sanitizer_options)

option(ROVER_ENABLE_ASAN "Enable AddressSanitizer" OFF)

if(ROVER_ENABLE_ASAN)
    if(MSVC)
        target_compile_options(rover_sanitizer_options INTERFACE /fsanitize=address)
    else()
        target_compile_options(rover_sanitizer_options INTERFACE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(rover_sanitizer_options INTERFACE -fsanitize=address)
    endif()
endif()
