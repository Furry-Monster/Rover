add_library(rover_core INTERFACE)
add_library(Rover::Core ALIAS rover_core)

target_include_directories(rover_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(rover_core INTERFACE
    Rover::CompilerOptions
    Rover::SanitizerOptions
)

set_target_properties(rover_core PROPERTIES FOLDER "Core")
