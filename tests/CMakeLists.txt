if (NOT ROVER_BUILD_TESTS)
    return()
endif ()

# ---------------------------------------------------------------------------
# Helper: add one test source file as a CTest case.
#
#   rover_add_test(test_foo)
#     → compiles tests/test_foo.cpp
#     → registers CTest case "test_foo"
# ---------------------------------------------------------------------------
function(rover_add_test _name)
    add_executable(${_name} ${_name}.cpp)
    rover_target_common_setup(${_name} FOLDER "Tests")
    target_link_libraries(${_name} PRIVATE
        Rover::Core
        Rover::Vendor
        Rover::Doctest
    )
    target_include_directories(${_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}/generated
        ${CMAKE_SOURCE_DIR}/vendor/doctest
    )
    add_test(NAME ${_name} COMMAND ${_name})
endfunction()

rover_add_test(test_string_name)
rover_add_test(test_memory)
rover_add_test(test_error)
rover_add_test(test_object)
