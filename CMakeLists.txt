cmake_minimum_required(VERSION 3.14)
project(tinykv)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# for google benchmark
set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON) # Allow dependency download

# Add source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Define the executable
add_executable(tinykv ${SOURCES})

# Optionally add include directories
target_include_directories(tinykv PRIVATE src)

# UNCOMMENT BELOW IF YOU WANT TO BENCHMARK
# # --- Google Benchmark (add the library) ---
# add_subdirectory(benchmark)

# # ------------------------
# # Benchmarks (tinykv_benchmark)
# # ------------------------
# # Exclude src/main.cpp from benchmark sources, else have 2 main.cpp definitions
# file(GLOB_RECURSE BENCH_SOURCES "tinykv_benchmarks/*.cpp")
# file(GLOB_RECURSE NON_MAIN_SOURCES "src/*.cpp")
# list(FILTER NON_MAIN_SOURCES EXCLUDE REGEX ".*main.cpp")

# add_executable(tinykv_benchmark ${BENCH_SOURCES} ${NON_MAIN_SOURCES})

# # Add include dirs for both your src and Google Benchmark
# target_include_directories(tinykv_benchmark PRIVATE src)

# # Link against Google Benchmark
# target_link_libraries(tinykv_benchmark PRIVATE benchmark::benchmark)