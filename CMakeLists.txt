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

target_link_libraries(postgresBenchmark pq)
