cmake_minimum_required(VERSION 3.12)

project(
    allocator_tst
        LANGUAGES CXX
)

add_executable(samples samples.cpp)

find_package(Threads REQUIRED)
find_package(
    Boost COMPONENTS
        unit_test_framework REQUIRED
        log_setup REQUIRED
        log REQUIRED
)

set_target_properties(
    samples
        PROPERTIES
            CXX_STANDARD 17
            CXX_EXTENSIONS OFF
            COMPILE_OPTIONS "-g;-Wpedantic;-Wall;-Wextra;-Werror"
            COMPILE_DEFINITIONS BOOST_ALL_DYN_LINK
            LINK_LIBRARIES "lib;boost_unit_test_framework"
            BUILD_RPATH "${CMAKE_BINARY_DIR}/lib"
            INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
)



add_test(
    NAME trivial
        COMMAND samples
)

include(CTest)
