cmake_minimum_required(VERSION 3.12)

project(
    allocator_src
        LANGUAGES CXX
)

add_executable(allocator_src main.cpp)

set_target_properties(
    allocator_src
        PROPERTIES
            CXX_STANDARD 17
            CXX_EXTENSIONS OFF
            CXX_STANDARD_REQUIRED ON
            COMPILE_OPTIONS "-Wpedantic;-Wall;-Wextra;-Werror;"
            LINK_LIBRARIES "lib;${CMAKE_THREAD_LIBS_INIT}"
            BUILD_RPATH "${CMAKE_BINARY_DIR}/lib"
            INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib"
)

install(
    TARGETS allocator_src
        RUNTIME DESTINATION bin
        COMPONENT prog
)
