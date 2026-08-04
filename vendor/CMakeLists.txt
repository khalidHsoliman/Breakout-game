set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

add_library(glad STATIC
    glad/src/gl.c)
target_include_directories(glad SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/glad/include")

set_target_properties(glfw glad PROPERTIES FOLDER "vendor")
