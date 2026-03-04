cmake_minimum_required(VERSION 3.19)
project(ccsds122x0b2_impl
    HOMEPAGE_URL "https://github.com/maxim-komlik/ccsds122x0b2_impl"
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(REPO_ROOT_DIR "${PROJECT_SOURCE_DIR}/../../")
set(REPO_SOURCE_DIR "${REPO_ROOT_DIR}src/")
set(REPO_BUILD_DIR "${REPO_ROOT_DIR}/build/")
set(REPO_OUT_DIR "${REPO_BUILD_DIR}/out/")
set(REPO_DOC_DIR "${REPO_ROOT_DIR}/doc/")

option(BUILD_FRONTEND "Build front-end interfaces (e.g. cli)" ON)
option(BUILD_TESTS "Build tests" OFF)

if (${BUILD_FRONTEND} AND ${MSVC})
    # cli interface cannot be built with _DEBUG, because it uses constexpr std::vector(), and 
    # it allocates memory when _DEBUG is set.

    list(REMOVE_ITEM CMAKE_CXX_FLAGS_DEBUG "_DEBUG")
    # set(CMAKE_VS_USE_DEBUG_LIBRARIES false)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")        
endif()


add_subdirectory(common)
add_subdirectory(core)

if (${BUILD_FRONTEND})
    add_subdirectory(cli)
endif()

if (${BUILD_TESTS})
    find_package(GTest REQUIRED)
    add_subdirectory(test)
endif()
