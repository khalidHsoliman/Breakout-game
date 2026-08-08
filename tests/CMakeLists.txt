include(FetchContent)

FetchContent_Declare(googletest
    URL      https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926)

# MSVC: build GoogleTest against the same dynamic CRT as the rest of the project.
set(gtest_force_shared_crt ON  CACHE BOOL "" FORCE)
set(INSTALL_GTEST          OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(breakout_tests
    smoke_tests.cpp
    math_tests.cpp
    component_store_tests.cpp
    world_tests.cpp
    collision_tests.cpp
    systems_tests.cpp
    level_tests.cpp
    spawn_tests.cpp
    input_tests.cpp
    font_tests.cpp
    state_tests.cpp)

target_link_libraries(breakout_tests PRIVATE
    GTest::gtest_main
    breakout_game
    breakout_options)

include(GoogleTest)
gtest_discover_tests(breakout_tests)

set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES FOLDER "vendor")
