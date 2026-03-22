cmake_minimum_required(VERSION 3.16)
project(challenge-12-feed-arbiter)

add_executable(benchmark benchmark.cpp solution/solution.cpp)

include(../common/challenge.cmake)
