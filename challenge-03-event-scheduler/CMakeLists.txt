cmake_minimum_required(VERSION 3.16)
project(challenge-03-event-scheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include common harness from parent
include_directories(${CMAKE_SOURCE_DIR}/..)
include_directories(${CMAKE_SOURCE_DIR})

set(CMAKE_CXX_FLAGS_RELEASE "-O2")

add_executable(benchmark
    benchmark.cpp
    solution/solution.cpp
)
