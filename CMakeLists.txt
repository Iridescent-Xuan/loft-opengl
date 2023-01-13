cmake_minimum_required(VERSION 3.20)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(cg_final VERSION 1.0)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 14)

file(COPY "media/" DESTINATION "media")

add_subdirectory(./external/glad)
set_target_properties(glad PROPERTIES FOLDER "lib")

add_subdirectory(./external/glm)

set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "utility")

set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
add_subdirectory(./external/glfw)
set_target_properties(glfw PROPERTIES FOLDER "lib/glfw")
set_target_properties(uninstall PROPERTIES FOLDER "lib/glfw")

add_subdirectory(./external/imgui)
set_target_properties(imgui PROPERTIES FOLDER "lib")

add_subdirectory(./external/stb)
set_target_properties(stb PROPERTIES FOLDER "lib")

add_subdirectory(./src)