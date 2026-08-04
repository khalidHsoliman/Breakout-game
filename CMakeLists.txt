cmake_minimum_required(VERSION 3.21)

# C is required by the vendored GLFW and glad sources.
project(Breakout
    VERSION   0.1.0
    LANGUAGES C CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Use: cmake -S . -B build")
endif()

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(BREAKOUT_BUILD_TESTS "Build the unit tests" ON)

# Applied to our targets only, so vendored code keeps its own warning level.
add_library(breakout_options INTERFACE)
if(MSVC)
    target_compile_options(breakout_options INTERFACE /W4 /permissive- /utf-8 /MP)
else()
    target_compile_options(breakout_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(vendor)
add_subdirectory(src)

if(BREAKOUT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT breakout)
