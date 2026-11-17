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
    ball_speed_tests.cpp
    progression_tests.cpp
    level_check_tests.cpp
    level_files_tests.cpp
    power_up_tests.cpp
    laser_tests.cpp
    steel_tests.cpp
    stuck_ball_tests.cpp
    replay_tests.cpp)

target_link_libraries(breakout_tests PRIVATE
    GTest::gtest_main
    breakout_game
    breakout_options)

include(GoogleTest)
gtest_discover_tests(breakout_tests)

# breakout_replay itself, run through a script so the exit code and the whole
# output are both checked. different.txt is good.txt with ten more steps
# after the game over, so it ends before its last step: DIFFERENT, though all
# the numbers on its end line are right. The good file's game never touches the paddle after
# a launch and catches nothing, so no sin or cos decides it and it should end
# the same with any compiler.
#
# laser.txt is a game in which a laser capsule is caught and its beams break
# bricks. The ball gets above the bricks through the gap in the middle and
# bounces there long enough for capsules to reach the paddle, which keeps out
# of its way: it never touches the paddle and there is never a second ball, so
# no sin or cos decides this one either. old.txt is good.txt as version 1
# recorded it, from before the laser: refused, not played by the new rules.
#
# All but old.txt were recorded through game::Recorder and written by
# to_text, not by hand; bad.txt is then good.txt with one key made unknown.
set(replays "${CMAKE_CURRENT_SOURCE_DIR}/replays")

add_test(NAME BreakoutReplay.PlaysAGoodFile
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:breakout_replay>"
        "-DREPLAY=${replays}/good.txt"
        -DEXIT_CODE=0
        "-DEXPECTED_STDOUT=${replays}/good.stdout"
        -P "${replays}/run_replay.cmake")

add_test(NAME BreakoutReplay.SaysWhenTheEndIsDifferent
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:breakout_replay>"
        "-DREPLAY=${replays}/different.txt"
        -DEXIT_CODE=2
        "-DEXPECTED_STDOUT=${replays}/different.stdout"
        -P "${replays}/run_replay.cmake")

add_test(NAME BreakoutReplay.PlaysALaserGame
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:breakout_replay>"
        "-DREPLAY=${replays}/laser.txt"
        -DEXIT_CODE=0
        "-DEXPECTED_STDOUT=${replays}/laser.stdout"
        -P "${replays}/run_replay.cmake")

add_test(NAME BreakoutReplay.RefusesAnOlderVersion
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:breakout_replay>"
        "-DREPLAY=${replays}/old.txt"
        -DEXIT_CODE=1
        "-DEXPECTED_STDERR=${replays}/old.stderr"
        -P "${replays}/run_replay.cmake")

add_test(NAME BreakoutReplay.RefusesABadFile
    COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:breakout_replay>"
        "-DREPLAY=${replays}/bad.txt"
        -DEXIT_CODE=1
        "-DEXPECTED_STDERR=${replays}/bad.stderr"
        -P "${replays}/run_replay.cmake")

set_target_properties(gtest gtest_main PROPERTIES FOLDER "vendor")
