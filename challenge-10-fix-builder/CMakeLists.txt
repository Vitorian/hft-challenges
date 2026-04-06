cmake_minimum_required(VERSION 3.20)
project(challenge-10-fix-builder LANGUAGES CXX)

add_executable(benchmark benchmark.cpp solution/solution.cpp)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../common/challenge.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../common/challenge.cmake)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/common/challenge.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/common/challenge.cmake)
endif()
