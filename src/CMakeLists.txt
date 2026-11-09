add_library(breakout_math INTERFACE)
target_include_directories(breakout_math INTERFACE "${CMAKE_SOURCE_DIR}/include")

add_library(breakout_core INTERFACE)
target_include_directories(breakout_core INTERFACE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(breakout_core INTERFACE breakout_math)

add_library(breakout_game STATIC
    game/World.cpp
    game/Systems.cpp
    game/Level.cpp
    game/Spawn.cpp
    game/LevelLayout.cpp
    game/LevelCheck.cpp
    game/LevelFiles.cpp
    game/Editor.cpp)

target_include_directories(breakout_game PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(breakout_game
    PUBLIC  breakout_core
    PRIVATE breakout_options)

# Everything below this line is the window, sound and executable side.
if(NOT BREAKOUT_BUILD_PLATFORM)
    return()
endif()

add_library(breakout_platform STATIC
    platform/Window.cpp
    platform/Renderer.cpp
    platform/FileSystem.cpp
    platform/Audio.cpp)

target_include_directories(breakout_platform PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(breakout_platform
    PUBLIC  breakout_core
    PRIVATE glfw glad miniaudio breakout_options)

add_executable(breakout
    main.cpp)

target_include_directories(breakout PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_compile_definitions(breakout PRIVATE $<$<CONFIG:Debug>:BREAKOUT_DEBUG>)
target_link_libraries(breakout PRIVATE breakout_platform breakout_game breakout_options)

set_target_properties(breakout PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# The game looks for assets beside its own executable, not beside the working
# directory, so they have to be put there. Copied rather than referenced from
# the source tree, so the build output is the whole game and can be zipped.
add_custom_command(TARGET breakout POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:breakout>/assets"
    COMMENT "Copying assets beside the executable")
