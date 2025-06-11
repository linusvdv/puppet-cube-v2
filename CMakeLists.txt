cmake_minimum_required(VERSION 3.16)

include(FetchContent)

project(PuppetCubeV2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GUI "Enable Graphical User Interface features" ON)

if(GUI)
    add_compile_definitions(GUI)
endif()

# pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# parallel hashmap by Gregory Popovitch
include(FetchContent)
FetchContent_Declare(
    parallel-hashmap
    GIT_REPOSITORY https://github.com/greg7mdp/parallel-hashmap.git
    GIT_TAG        v1.3.12
)
FetchContent_MakeAvailable(parallel-hashmap)
include_directories(${parallel-hashmap_SOURCE_DIR})

include_directories(include/)

add_executable(PuppetCubeV2
    src/main.cpp
    src/logger.cpp
    src/settings.cpp
    src/cube.cpp
    src/corner_orientation.cpp
    src/corner_position.cpp
    src/corner_heuristic.cpp
    src/edge_orientation.cpp
    src/edge_position.cpp
    src/tablebase.cpp
)

target_link_libraries(PuppetCubeV2 PRIVATE Threads::Threads)

set(Debug_Options -Wextra -g -O0 -fno-inline -fno-omit-frame-pointer)
set(Release_Options -Wextra -g -O3 -flto)

target_compile_options(PuppetCubeV2 PRIVATE -Wall
    $<$<CONFIG:Debug>:${Debug_Options}>
    $<$<CONFIG:Release>:${Release_Options}>
)
