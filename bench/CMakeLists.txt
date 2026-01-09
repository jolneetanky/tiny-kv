# bench/CMakeLists.txt

add_executable(bench_compaction
  bench_compaction.cpp
)

target_include_directories(bench_compaction
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(bench_compaction
  PRIVATE
    tinykv_core
    benchmark::benchmark
)

# Optional: nicer warnings for benchmarks
target_compile_options(bench_compaction PRIVATE -Wall -Wextra)
