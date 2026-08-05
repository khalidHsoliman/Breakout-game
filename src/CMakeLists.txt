add_library(breakout_math INTERFACE)
target_include_directories(breakout_math INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(breakout
    main.cpp)

target_include_directories(breakout PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(breakout PRIVATE breakout_options)

set_target_properties(breakout PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
