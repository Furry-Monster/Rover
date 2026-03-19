add_library(rover_core INTERFACE)
add_library(Rover::Core ALIAS rover_core)

target_include_directories(rover_core INTERFACE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(rover_core INTERFACE
    Rover::CompilerOptions
    Rover::SanitizerOptions
)

add_library(rover_core_impl STATIC
    error/error_list.cpp
    memory/memory.cpp
    string/string_name.cpp
)
target_link_libraries(rover_core_impl PUBLIC rover_core)
target_link_libraries(rover_core INTERFACE rover_core_impl)

set_target_properties(rover_core rover_core_impl PROPERTIES FOLDER "Core")
