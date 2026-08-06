add_library(breakout_math INTERFACE)
target_include_directories(breakout_math INTERFACE "${CMAKE_SOURCE_DIR}/include")

add_library(breakout_core INTERFACE)
target_include_directories(breakout_core INTERFACE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(breakout_core INTERFACE breakout_math)

add_library(breakout_platform STATIC
    platform/Window.cpp
    platform/Renderer.cpp)

target_include_directories(breakout_platform PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(breakout_platform
    PUBLIC  breakout_core
    PRIVATE glfw glad breakout_options)

add_executable(breakout
    main.cpp)

target_include_directories(breakout PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(breakout PRIVATE breakout_platform breakout_options)

set_target_properties(breakout PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
