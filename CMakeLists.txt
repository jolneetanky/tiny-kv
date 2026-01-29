# cmake_minimum_required(VERSION 3.20)
# project(tinykv CXX)
# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# # TESTING PURPOSES; TO INCLUDE GOOGLETEST
# # ---
# include(FetchContent)

# FetchContent_Declare(
#   googletest
#   URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
# )

# # Prevent GoogleTest from installing itself
# set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# FetchContent_MakeAvailable(googletest)
# # ---

# enable_testing()

# add_subdirectory(src)
# add_subdirectory(tests)

# set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
# set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
# set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
# add_subdirectory(external/spdlog)

# option(TINYKV_BUILD_BENCHMARKS "Build the standalone benchmark executable" ON)
# if (TINYKV_BUILD_BENCHMARKS)
#   add_executable(tinykv_benchmark tinykv_benchmarks/main.cpp)
#   target_link_libraries(tinykv_benchmark PRIVATE tinykv_core)
# endif()

cmake_minimum_required(VERSION 3.20)
project(tinykv CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------
# Dependencies
# ------------------------
include(FetchContent)

# GoogleTest (for tests)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Google Benchmark (for benchmarks)
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.9.1
)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

# spdlog
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
add_subdirectory(external/spdlog)

# ------------------------
# Subprojects
# ------------------------
enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

option(TINYKV_BUILD_BENCHMARKS "Build TinyKV benchmarks" ON)
if (TINYKV_BUILD_BENCHMARKS)
  add_subdirectory(bench)
endif()
