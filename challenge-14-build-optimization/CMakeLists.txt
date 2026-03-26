cmake_minimum_required(VERSION 3.20)
project(challenge-14-build-optimization LANGUAGES CXX)

# Read user's build config
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/solution/build-config.yaml" BUILD_CONFIG)

# Parse compiler_flags
string(REGEX MATCH "compiler_flags:[ \t]*\"([^\"]*)\"" _match "${BUILD_CONFIG}")
set(USER_COMPILER_FLAGS "${CMAKE_MATCH_1}")

# Parse linker_flags
string(REGEX MATCH "linker_flags:[ \t]*\"([^\"]*)\"" _match "${BUILD_CONFIG}")
set(USER_LINKER_FLAGS "${CMAKE_MATCH_1}")

# Validate: no -O flags
string(REGEX MATCH "-O[0-3sfg]|Ofast" _bad_flag "${USER_COMPILER_FLAGS}")
if(_bad_flag)
    message(FATAL_ERROR "Optimization level flags (-O0/-O1/-O2/-O3/-Ofast/-Os/-Og) are not allowed. Use individual optimization flags instead.")
endif()

# Validate: character limit
string(LENGTH "${USER_COMPILER_FLAGS}" _clen)
string(LENGTH "${USER_LINKER_FLAGS}" _llen)
math(EXPR _total "${_clen} + ${_llen}")
if(_total GREATER 256)
    message(FATAL_ERROR "Total flag length (${_total}) exceeds 256 character limit.")
endif()

# Parse defines (simple key: 'value' extraction)
string(REGEX MATCHALL "[A-Z_]+:[ \t]*'[^']*'" _defines "${BUILD_CONFIG}")
set(USER_DEFINES "")
foreach(_def ${_defines})
    string(REGEX MATCH "([A-Z_]+):[ \t]*'([^']*)'" _m "${_def}")
    if(CMAKE_MATCH_1 AND CMAKE_MATCH_2)
        list(APPEND USER_DEFINES "-D${CMAKE_MATCH_1}=${CMAKE_MATCH_2}")
    endif()
endforeach()

add_executable(benchmark
    benchmark.cpp
    pipeline.cpp
    processor.cpp
    math_ops.cpp
    dispatch.cpp
)

target_compile_features(benchmark PRIVATE cxx_std_20)

# Apply user flags
if(USER_COMPILER_FLAGS)
    separate_arguments(USER_COMPILER_FLAGS_LIST UNIX_COMMAND "${USER_COMPILER_FLAGS}")
    target_compile_options(benchmark PRIVATE ${USER_COMPILER_FLAGS_LIST})
endif()

if(USER_LINKER_FLAGS)
    separate_arguments(USER_LINKER_FLAGS_LIST UNIX_COMMAND "${USER_LINKER_FLAGS}")
    target_link_options(benchmark PRIVATE ${USER_LINKER_FLAGS_LIST})
endif()

if(USER_DEFINES)
    target_compile_definitions(benchmark PRIVATE ${USER_DEFINES})
endif()

target_include_directories(benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/solution
)

# Include common challenge cmake if available
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../common/challenge.cmake)
    # Don't include — we manage flags ourselves for this challenge
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/common/challenge.cmake)
    # Don't include
endif()

message(STATUS "Build Optimization Challenge")
message(STATUS "  Compiler flags (${_clen} chars): ${USER_COMPILER_FLAGS}")
message(STATUS "  Linker flags (${_llen} chars): ${USER_LINKER_FLAGS}")
message(STATUS "  Total: ${_total}/256 chars")
message(STATUS "  Defines: ${USER_DEFINES}")
