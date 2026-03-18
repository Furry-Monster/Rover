if (ROVER_BUILD_TESTS)
    enable_testing()
    file(GLOB _test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
    if (_test_sources)
        add_executable(RoverTests ${_test_sources})
        rover_target_common_setup(RoverTests FOLDER "Tests")
        target_include_directories(RoverTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_BINARY_DIR}/generated
        )
        target_link_libraries(RoverTests PRIVATE Rover::Core Rover::Vendor)
        add_test(NAME RoverTests COMMAND RoverTests)
    else ()
        message(STATUS "[Rover] tests/: no sources, skipping test executable")
    endif ()
endif ()
