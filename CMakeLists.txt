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
    src/tablebase.cpp
    src/random_position.cpp
    src/thread_pool.cpp
    src/search.cpp
    src/special_cubes.cpp
    src/info.cpp
    src/duplicate_rotations.cpp
    src/transposition_table.cpp
    src/rotation.cpp
    src/edge.cpp
    src/corner.cpp
)

if(USE_CUDA)
    add_compile_definitions(USE_CUDA)

    target_sources(PuppetCubeV2 PRIVATE
        src/corner.cu
        src/edge.cu
        src/info.cu
        src/search.cu
        src/tablebase.cu
    )

    set(CUDA_KEEP_DIR "${CMAKE_BINARY_DIR}/comp")
    file(MAKE_DIRECTORY ${CUDA_KEEP_DIR})

    target_link_libraries(PuppetCubeV2 PRIVATE Threads::Threads)

    target_compile_options(PuppetCubeV2 PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:-Wall -Wextra --expt-extended-lambda -diag-suppress 68 --keep --keep-dir=${CUDA_KEEP_DIR} -lineinfo>
        $<$<CONFIG:Release>:$<$<COMPILE_LANGUAGE:CUDA>:-g>>
        $<$<CONFIG:Debug>:$<$<COMPILE_LANGUAGE:CUDA>:-G>>
    )

    target_link_options(PuppetCubeV2 PRIVATE
    $<DEVICE_LINK:-Xptxas=-v>
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

set(Debug_Options -O0 -fno-inline -fno-omit-frame-pointer -Wshadow -Wall -Wextra)
set(Release_Options -O3 -Wshadow -Wall -Wextra)

target_compile_options(PuppetCubeV2 PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -g>
    $<$<CONFIG:Debug>:${Debug_Options}>
    $<$<CONFIG:Release>:${Release_Options}>
)

# ============
# LTO (Release only)
# ============
# Enables interprocedural/link-time optimization for CXX, and -- since this
# is a whole-target property, not scoped per language -- for CUDA device code
# too (nvcc's device LTO has been supported by CMake since 3.25). That's
# intentional here: we want both. The IPO check below covers CUDA explicitly
# so a build would warn instead of silently doing the wrong thing if device
# LTO ever weren't supported by the toolchain in use.
#
# If you ever want CXX LTO WITHOUT CUDA device LTO, this property can't be
# scoped to one language on a mixed target -- the CUDA sources would need to
# move into a separate target that doesn't have this property set.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)

    set(ipo_languages CXX)
    if(USE_CUDA)
        list(APPEND ipo_languages CUDA)
    endif()

    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES ${ipo_languages})
    if(ipo_supported)
        set_property(TARGET PuppetCubeV2 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "LTO enabled (${ipo_languages})")
    else()
        message(WARNING "LTO not supported: ${ipo_error}")
    endif()
endif()
