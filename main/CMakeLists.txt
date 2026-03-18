# main: entry point, main loop
file(GLOB _main_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
if (_main_sources)
    add_executable(RoverMain ${_main_sources})
    rover_target_common_setup(RoverMain FOLDER "Main")
    target_include_directories(RoverMain PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(RoverMain PRIVATE
        Rover::Core Rover::Scene Rover::Platform Rover::Servers Rover::Vendor
    )
else ()
    message(STATUS "[Rover] main/: no sources, skipping executable")
endif ()
