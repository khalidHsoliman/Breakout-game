set(gtest_force_shared_crt ON  CACHE BOOL "" FORCE)   # MSVC: same dynamic CRT as the rest
set(INSTALL_GTEST          OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK            OFF CACHE BOOL "" FORCE)   # only gtest is used

add_subdirectory("${CMAKE_SOURCE_DIR}/vendor/googletest" googletest EXCLUDE_FROM_ALL)

add_executable(breakout_tests
    math_tests.cpp
    component_store_tests.cpp
    world_tests.cpp
    collision_tests.cpp
    systems_tests.cpp
    level_tests.cpp
    spawn_tests.cpp
    input_tests.cpp
    font_tests.cpp
    editor_tests.cpp
    viewport_tests.cpp
    state_tests.cpp
    events_tests.cpp
    ball_speed_tests.cpp)

target_link_libraries(breakout_tests PRIVATE
    GTest::gtest_main
    breakout_game
    breakout_options)

include(GoogleTest)
gtest_discover_tests(breakout_tests)

set_target_properties(gtest gtest_main PROPERTIES FOLDER "vendor")
