cmake_minimum_required(VERSION 3.20)

project(loft)

set(THIRD_PARTY_LIBRARY_PATH ${CMAKE_SOURCE_DIR}/external)

file(GLOB PROJECT_HDR ./*.h)
file(GLOB PROJECT_SRC ./*.cpp)

set(BASE_HDR ./base/application.h
             ./base/frame_rate_indicator.h
             ./base/input.h
             ./base/glsl_program.h
             ./base/camera.h
             ./base/frustum.h
             ./base/framebuffer.h
             ./base/fullscreen_quad.h
             ./base/plane.h
             ./base/transform.h
             ./base/bounding_box.h
             ./base/vertex.h
             ./base/light.h
             ./base/texture.h
             ./base/texture2d.h
             ./base/texture_cubemap.h)

set(BASE_SRC ./base/application.cpp 
             ./base/glsl_program.cpp 
             ./base/camera.cpp 
             ./base/transform.cpp
             ./base/texture.cpp
             ./base/texture2d.cpp
             ./base/texture_cubemap.cpp
             ./base/fullscreen_quad.cpp)

add_executable(loft ${PROJECT_SRC} ${PROJECT_HDR} ${BASE_SRC} ${BASE_HDR})

if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
elseif(XCODE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
else()
    if (CMAKE_BUILD_TYPE MATCHES Debug)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Debug")
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Release")
    endif()
endif()

target_include_directories(loft PUBLIC ${THIRD_PARTY_LIBRARY_PATH}/glm)
target_include_directories(loft PUBLIC ${THIRD_PARTY_LIBRARY_PATH}/glad/include)
target_include_directories(loft PUBLIC ${THIRD_PARTY_LIBRARY_PATH}/glfw/include)
target_include_directories(loft PUBLIC ${THIRD_PARTY_LIBRARY_PATH}/imgui)
target_include_directories(loft PUBLIC ${THIRD_PARTY_LIBRARY_PATH}/stb)
# target_include_directories(loft PUBLIC ${CMAKE_SOURCE_DIR}/freeglut/include/GL)

target_link_libraries(loft glm)
target_link_libraries(loft glad)
target_link_libraries(loft glfw)
target_link_libraries(loft imgui)
target_link_libraries(loft stb)
# target_link_libraries(loft freeglut)