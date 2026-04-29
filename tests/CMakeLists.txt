message(STATUS "[Rover] Configuring tests...")

# ---------------------------------------------------------------------------
# rover_tests -- doctest-based unit/integration test runner.
#
# Each subsystem is expected to drop *_test.cpp files into this directory
# (or any subdirectory). A dedicated main() lives in tests/main.cpp.
# ---------------------------------------------------------------------------
rover_glob_sources(_test_sources "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(rover_tests ${_test_sources})

target_link_libraries(rover_tests
    PRIVATE
        Rover::CompileFlags
        Rover::Core
        Rover::Services
        Rover::Modules
        Rover::Platform
        Rover::Doctest
)

target_include_directories(rover_tests
    PRIVATE
        "${ROVER_ROOT_DIR}"
        "${ROVER_GENERATED_DIR}"
)

set_target_properties(rover_tests PROPERTIES
    FOLDER "Rover"
    OUTPUT_NAME "rover_tests"
)

add_test(NAME rover_tests COMMAND rover_tests)
