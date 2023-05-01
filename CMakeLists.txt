cmake_minimum_required(VERSION 3.0.0)
project(server_uring VERSION 0.1.0)

set (CMAKE_C_COMPILER "/usr/bin/gcc")
set (CMAKE_CXX_COMPILER "/usr/bin/g++")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -luring -lfmt -Wall")

include_directories("./include")

add_executable(server_uring main.cpp)

target_link_libraries(server_uring fmt uring)
