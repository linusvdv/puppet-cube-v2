cmake_minimum_required(VERSION 3.23.1 FATAL_ERROR)

include(FetchContent)

project(PuppetCubeV2)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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


# ===========================================
# Important option to enable the CUDA search!
# ===========================================
# - ON
# - OFF

option(USE_CUDA "Build with CUDA support" ON)

if(USE_CUDA)
    find_package(CUDAToolkit 12.6 REQUIRED)

    # Ensure the CUDA compiler matches the found toolkit version
    if(CUDAToolkit_FOUND AND NOT CMAKE_CUDA_COMPILER)
        # Use the nvcc from the same installation as the found toolkit
        set(CMAKE_CUDA_COMPILER "${CUDAToolkit_BIN_DIR}/nvcc")
        message(STATUS "Setting CUDA compiler to match toolkit: ${CMAKE_CUDA_COMPILER}")
    endif()

    # Verify the compiler exists
    if(CMAKE_CUDA_COMPILER AND NOT EXISTS ${CMAKE_CUDA_COMPILER})
        message(FATAL_ERROR "CUDA compiler not found at: ${CMAKE_CUDA_COMPILER}")
    endif()
 
    set(CMAKE_CUDA_SEPARABLE_COMPILATION ON)

    # cuda
    enable_language(CUDA)
    set(CMAKE_CUDA_USE_RESPONSE_FILE_FOR_INCLUDES OFF)

    # ========================
    # Architecture compilation
    # ========================
    set(CUDA_ARCH 89 CACHE STRING "Cuda architecture")
    set(CMAKE_CUDA_ARCHITECTURES ${CUDA_ARCH})

    set(CMAKE_CUDA_STANDARD 20)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
endif()

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
    src/BCHTSet.cpp
    src/random_position.cpp
    src/search.cpp
    src/special_cubes.cpp
    src/info.cpp
    src/duplicate_rotations.cpp
)

if(USE_CUDA)
    add_compile_definitions(USE_CUDA)

    target_sources(PuppetCubeV2 PRIVATE
        src/search.cu
        src/cube.cu
        src/info.cu
    )

    target_link_libraries(PuppetCubeV2 PRIVATE Threads::Threads)

    target_compile_options(PuppetCubeV2 PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:-Wall -Wextra -g --expt-extended-lambda -diag-suppress 68>
    )
else()
    target_link_libraries(PuppetCubeV2 PRIVATE Threads::Threads)
endif()


# ============
# Build Option
# ============
# - Release
# - Debug

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(Debug_Options -O0 -fno-inline -fno-omit-frame-pointer)
set(Release_Options -O3)

set(CUDA_KEEP_DIR "${CMAKE_BINARY_DIR}/comp")
file(MAKE_DIRECTORY ${CUDA_KEEP_DIR})

target_compile_options(PuppetCubeV2 PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -g>
    $<$<COMPILE_LANGUAGE:CUDA>:-Xptxas -v --keep --keep-dir=${CUDA_KEEP_DIR} -lineinfo>
    $<$<CONFIG:Debug>:${Debug_Options}>
    $<$<CONFIG:Debug>:$<$<COMPILE_LANGUAGE:CUDA>:-G>>
    $<$<CONFIG:Release>:${Release_Options}>
)
