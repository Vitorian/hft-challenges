cmake_minimum_required(VERSION 3.20)
project(challenge-18-plt-tax LANGUAGES CXX)

# --- libhotpath.so: built with FIXED flags. Solver flags do NOT apply here.
add_library(hotpath SHARED hotpath.cpp inner.cpp)
set_target_properties(hotpath PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
)
target_compile_options(hotpath PRIVATE -O3 -fPIC)

# --- benchmark: solver flags apply ONLY here.
add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark hotpath)

set_target_properties(benchmark PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
)

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
        if(_f MATCHES "^-O[0-3sg]?$"
           OR _f MATCHES "^-f[a-z][a-z0-9-]*(=[a-z0-9-]+)?$"
           OR _f MATCHES "^-m[a-z][a-z0-9-]*(=[a-z0-9-]+)?$"
           OR _f MATCHES "^-std=.*$")
            list(APPEND _safe_flags "${_f}")
        else()
            message(WARNING "Challenge 18: rejected flag: ${_f}")
        endif()
    endforeach()

    list(JOIN _safe_flags " " _user_flags)
    if(_user_flags)
        set_source_files_properties(benchmark.cpp PROPERTIES COMPILE_FLAGS "${_user_flags}")
        message(STATUS "Challenge 18: CXXFLAGS=${_user_flags}")
    endif()

    if(_user_flags)
        set_source_files_properties(hotpath.cpp inner.cpp PROPERTIES COMPILE_FLAGS "${_user_flags}")
        message(STATUS "Challenge 18: applied flags to libhotpath.so sources")
    endif()
endif()

# Link benchmark as non-PIE so -fno-pie codegen actually changes anything.
target_link_options(benchmark PRIVATE -no-pie)
