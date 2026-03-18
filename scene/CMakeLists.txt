# scene: scene tree, resources, 2d, 3d (Godot-inspired)
add_library(rover_scene INTERFACE)
add_library(Rover::Scene ALIAS rover_scene)

target_include_directories(rover_scene INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rover_scene INTERFACE Rover::Core)

set_target_properties(rover_scene PROPERTIES FOLDER "Scene")
