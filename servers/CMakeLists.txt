# servers: rendering, audio, physics (Godot-inspired)
add_library(rover_servers INTERFACE)
add_library(Rover::Servers ALIAS rover_servers)

target_include_directories(rover_servers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rover_servers INTERFACE Rover::Core Rover::Vendor)

set_target_properties(rover_servers PROPERTIES FOLDER "Servers")
