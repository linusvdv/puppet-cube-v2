cmake_minimum_required(VERSION 3.16)

include(FetchContent)

project(PuppetCubeV2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS})

# glfw
FetchContent_Declare(glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw"
    GIT_TAG "3.4"
    GIT_SHALLOW ON
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# glm
FetchContent_Declare(
    glm
    GIT_REPOSITORY "https://github.com/g-truc/glm.git"
    GIT_TAG "1.0.1"
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(glm)

# glad
add_library(glad src/glad.c)
target_include_directories(glad PUBLIC include/)
target_include_directories(glad PRIVATE src/)

add_executable(PuppetCubeV2
    src/main.cpp
    src/error_handler.cpp
    src/settings.cpp
    src/rotation.cpp
    src/window_manager.cpp
    src/shader.cpp
    src/renderer.cpp
    src/mesh.cpp
)

target_link_libraries(PuppetCubeV2 PRIVATE glfw glad glm Threads::Threads)

target_compile_features(PuppetCubeV2 PRIVATE cxx_std_20)

target_compile_options(PuppetCubeV2 PRIVATE -Wall -Wextra -g)
