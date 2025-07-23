cmake_minimum_required(VERSION 3.16)

include(FetchContent)

project(PuppetCubeV2)

enable_language(CUDA)

set(CMAKE_CUDA_ARCHITECTURES 89)

set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    src/edge_heuristic.cpp
    src/tablebase.cpp
    src/search.cu
)

target_link_libraries(PuppetCubeV2 PRIVATE Threads::Threads)

set(Debug_Options -O0 -fno-inline -fno-omit-frame-pointer)
set(Release_Options -O3)

target_compile_options(PuppetCubeV2 PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -g -std=c++20>
    $<$<COMPILE_LANGUAGE:CUDA>:-Wall -Wextra -g -std=c++20>
    $<$<CONFIG:Debug>:${Debug_Options}>
    $<$<CONFIG:Release>:${Release_Options}>
)
