cmake_minimum_required(VERSION 3.20)
project(challenge-06-seqlock LANGUAGES CXX)

add_executable(benchmark benchmark.cpp solution/solution.cpp)

# common/ is a sibling locally, but copied inside during certified runs
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../common/challenge.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../common/challenge.cmake)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/common/challenge.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/common/challenge.cmake)
endif()
