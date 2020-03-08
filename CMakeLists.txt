cmake_minimum_required(VERSION 3.12)

set( PROJECT_BUILD_NUMBER   $ENV{TRAVIS_BUILD_NUMBER})
set( STATIC_POOL_SIZE       $ENV{ALLOCATOR_STATIC_POOL_SIZE})
set( STATIC_POOL_CHUNK_SIZE $ENV{ALLOCATOR_STATIC_POOL_CHUNK_SIZE})

if ("${PROJECT_BUILD_NUMBER}" STREQUAL "")
    set(PROJECT_BUILD_NUMBER "1")
endif ("${PROJECT_BUILD_NUMBER}" STREQUAL "")

if ("${STATIC_POOL_SIZE}" STREQUAL "")
    set(STATIC_POOL_SIZE "1000")
endif ("${STATIC_POOL_SIZE}" STREQUAL "")

if ("${STATIC_POOL_CHUNK_SIZE}" STREQUAL "")
    set(STATIC_POOL_CHUNK_SIZE "10")
endif ("${STATIC_POOL_CHUNK_SIZE}" STREQUAL "")

project(
    allocator
        VERSION 0.0.${PROJECT_BUILD_NUMBER}
        LANGUAGES CXX
)

find_package(Threads REQUIRED)

configure_file(project_info.hpp.in project_info.hpp)

add_subdirectory(lib)
add_subdirectory(src)
#add_subdirectory(tst)
#add_subdirectory(pkg)

#enable_testing()
include(CTest)
