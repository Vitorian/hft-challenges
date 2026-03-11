cmake_minimum_required(VERSION 3.20)
project(challenge-01-order-book CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

find_package(benchmark REQUIRED)

add_executable(benchmark
    benchmark.cpp
    solution/solution.cpp
)

target_include_directories(benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/solution
)

target_link_libraries(benchmark benchmark::benchmark benchmark::benchmark_main)
