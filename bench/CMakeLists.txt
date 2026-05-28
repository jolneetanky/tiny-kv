add_executable(bench_read_heavy
  bench_read_heavy.cpp
)

target_include_directories(bench_read_heavy
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(bench_read_heavy
  PRIVATE
    tinykv_core
    benchmark::benchmark
    benchmark::benchmark_main
)

target_compile_options(bench_read_heavy PRIVATE -Wall -Wextra)
