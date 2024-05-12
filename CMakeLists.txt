cmake_minimum_required(VERSION 3.16)

project(PuppetCubeV2)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS})

# glfw
find_package(glfw3 REQUIRED)

add_library(glad src/glad.c)
target_include_directories(glad PUBLIC include/)
target_include_directories(glad PRIVATE src/)

add_executable(PuppetCubeV2
    src/main.cpp
    src/renderer.cpp
    src/shader.cpp
    src/settings.cpp
    src/error_handler.cpp
    src/cube.cpp)

target_link_libraries(PuppetCubeV2 PRIVATE glfw glad)

target_compile_features(PuppetCubeV2 PRIVATE cxx_std_20)

target_compile_options(PuppetCubeV2 PRIVATE -Wall -Wextra)
