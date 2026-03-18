# platform: platform abstraction, per-OS (Godot-inspired)
add_library(rover_platform INTERFACE)
add_library(Rover::Platform ALIAS rover_platform)

target_include_directories(rover_platform INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rover_platform INTERFACE Rover::Core Rover::Vendor)

set_target_properties(rover_platform PROPERTIES FOLDER "Platform")
