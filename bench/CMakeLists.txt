add_executable(bench_read_heavy
  bench_read_heavy.cpp
)

add_executable(bench_minimum
  bench_minimum.cpp
)

add_executable(memory_investigation
  memory_investigation.cpp
)

target_include_directories(bench_read_heavy
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(bench_minimum
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(memory_investigation
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(bench_read_heavy
  PRIVATE
    tinykv_core
    benchmark::benchmark
    benchmark::benchmark_main
)

target_link_libraries(bench_minimum
  PRIVATE
    tinykv_core
    benchmark::benchmark
    benchmark::benchmark_main
)

target_link_libraries(memory_investigation
  PRIVATE
    tinykv_core
)

target_compile_options(bench_read_heavy PRIVATE -Wall -Wextra)
target_compile_options(bench_minimum PRIVATE -Wall -Wextra)
target_compile_options(memory_investigation PRIVATE -Wall -Wextra)
