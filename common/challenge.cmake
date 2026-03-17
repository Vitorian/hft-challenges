# common/challenge.cmake — shared build config for all challenges
# Include this after add_executable(benchmark ...) in each challenge's CMakeLists.txt

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

target_include_directories(benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/solution
)

# Boost (header-only — intrusive containers, flat_map, etc.)
find_package(Boost QUIET)
if(Boost_FOUND)
    target_link_libraries(benchmark Boost::boost)
endif()

# Abseil (flat_hash_map, btree, etc.)
find_package(absl QUIET)
if(absl_FOUND)
    target_link_libraries(benchmark
        absl::flat_hash_map absl::node_hash_map
        absl::btree absl::hash absl::strings)
endif()

# TBB (scalable allocator, cache_aligned_allocator)
find_package(TBB QUIET)
if(TBB_FOUND)
    target_link_libraries(benchmark TBB::tbb)
endif()

# Allow solutions to specify additional link libraries via solution/libraries.cmake
# Example solution/libraries.cmake:
#   target_link_libraries(benchmark jemalloc)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/solution/libraries.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/solution/libraries.cmake)
endif()
