cmake_minimum_required(VERSION 3.20)
project(challenge-14-build-optimization LANGUAGES CXX)

add_executable(benchmark benchmark.cpp)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../common/challenge.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../common/challenge.cmake)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/common/challenge.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/common/challenge.cmake)
endif()

# Read CXXFLAGS from the first line of solution/solution.h: // CXXFLAGS=...
# Only whitelisted flag patterns are allowed (no shell injection).
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/solution/solution.h _first_line LIMIT_COUNT 1)
if(_first_line MATCHES "^// CXXFLAGS=(.*)")
    string(STRIP "${CMAKE_MATCH_1}" _raw_flags)
    separate_arguments(_flag_list UNIX_COMMAND "${_raw_flags}")

    set(_safe_flags "")
    foreach(_f IN LISTS _flag_list)
        # Whitelist: -O levels, -f flags, -m arch/tune flags, -march=, -mtune=, -std=
        if(_f MATCHES "^-O[0-3sg]?$"
           OR _f MATCHES "^-f[a-z][a-z0-9-]*$"
           OR _f MATCHES "^-m[a-z][a-z0-9-]*(=[a-z0-9-]+)?$"
           OR _f MATCHES "^-std=.*$")
            list(APPEND _safe_flags "${_f}")
        else()
            message(WARNING "Challenge 14: rejected flag: ${_f}")
        endif()
    endforeach()

    list(JOIN _safe_flags " " _user_flags)
    if(_user_flags)
        set_source_files_properties(benchmark.cpp PROPERTIES COMPILE_FLAGS "${_user_flags}")
        message(STATUS "Challenge 14: CXXFLAGS=${_user_flags}")
    endif()
endif()
