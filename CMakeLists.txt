cmake_minimum_required(VERSION 3.13)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(defaultBuildType)
cmake_set_default_build_type()

project(postgresBenchmark CXX)

include(utils)
include(sanitizers)

set(CMAKE_CXX_STANDARD 17)

add_executable_with_warnings(postgresBenchmark main.cpp)
enable_lto_in_release(postgresBenchmark)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif ()

target_link_libraries(postgresBenchmark pq)
