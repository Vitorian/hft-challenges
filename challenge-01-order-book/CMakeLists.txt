cmake_minimum_required(VERSION 3.20)
project(challenge-01-order-book LANGUAGES CXX)

add_executable(benchmark benchmark.cpp solution/solution.cpp)

include(${CMAKE_CURRENT_SOURCE_DIR}/../common/challenge.cmake)
