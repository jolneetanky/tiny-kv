# -------- GoogleTest --------
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Sanitizers (nice for debug)
set(TEST_COMPILE_FLAGS "-O0;-g;-fsanitize=address,undefined")
set(TEST_LINK_FLAGS    "-fsanitize=address,undefined")

# Common include dirs for tests
include_directories(
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/tests
)

# -------- sstable_file_manager_impl_test --------
add_executable(sstable_file_manager_impl_test
  ${CMAKE_CURRENT_LIST_DIR}/unit_tests/sstable_file_manager_impl_test.cpp

  # If these exist as .cpp files (not header-only), include them too:
  ${PROJECT_SOURCE_DIR}/tests/factories/mock_bloom_filter/mock_bloom_filter_factory.cpp
  ${PROJECT_SOURCE_DIR}/tests/mocks/bloom_filter.cpp
)

target_link_libraries(sstable_file_manager_impl_test
  PRIVATE tinykv_core GTest::gmock_main)  # or GTest::gtest_main if you don't use gmock

target_compile_options(sstable_file_manager_impl_test PRIVATE ${TEST_COMPILE_FLAGS})
target_link_options(   sstable_file_manager_impl_test PRIVATE ${TEST_LINK_FLAGS})

# GTest discovery
include(GoogleTest)
gtest_discover_tests(sstable_file_manager_impl_test)


# -------- memtable_impl_test --------
# add_executable(memtable_impl_test
#   ${CMAKE_CURRENT_LIST_DIR}/memtable_impl_test.cpp
#   # add any test-only .cpps here (if your mocks/factories have .cpp files)
#   # ${PROJECT_SOURCE_DIR}/tests/factories/mock_bloom_filter/mock_bloom_filter_factory.cpp
#   # ${PROJECT_SOURCE_DIR}/tests/mocks/bloom_filter.cpp
# )
# target_link_libraries(memtable_impl_test
#   PRIVATE tinykv_core GTest::gmock_main
# )
# target_compile_options(memtable_impl_test PRIVATE ${TEST_COMPILE_FLAGS})
# target_link_options(memtable_impl_test    PRIVATE ${TEST_LINK_FLAGS})

# -------- sstable_manager_impl_test --------
# add_executable(sstable_manager_impl_test
#   ${CMAKE_CURRENT_LIST_DIR}/unit_tests/sstable_manager_impl_test.cpp
#   # If your mock factory has a .cpp (not header-only), add it here:
#   ${PROJECT_SOURCE_DIR}/tests/factories/mock_bloom_filter/mock_bloom_filter_factory.cpp
#   # And any mock implementation .cpp:
#   # ${PROJECT_SOURCE_DIR}/tests/mocks/bloom_filter.cpp
# )
# target_link_libraries(sstable_manager_impl_test
#   PRIVATE tinykv_core GTest::gmock_main
# )
# target_compile_options(sstable_manager_impl_test PRIVATE ${TEST_COMPILE_FLAGS})
# target_link_options(sstable_manager_impl_test    PRIVATE ${TEST_LINK_FLAGS})

# -------- gtest discovery --------
# include(GoogleTest)
# gtest_discover_tests(memtable_impl_test)
# gtest_discover_tests(sstable_manager_impl_test)

# cmake_minimum_required(VERSION 3.20)
# project(tinykv CXX)

# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# # -------- GoogleTest via FetchContent --------
# include(FetchContent)
# FetchContent_Declare(
#   googletest
#   URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
# )
# set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(googletest)

# # -------- Your library (production sources) --------
# # List the .cpp files that implement the classes your tests include.
# # Adjust/expand this list to match your repo structure.
# add_library(tinykv_core STATIC
#   ../src/mem_table/mem_table_impl.cpp
#   ../src/skip_list/skip_list_impl.cpp
#   ../src/sstable_manager/sstable_manager_impl.cpp
#   ../src/sstable_file_manager/sstable_file_manager_impl.cpp
#   ../src/wal/wal.cpp
#   ../src/level_manager/level_manager_impl.cpp
#   ../src/bloom_filter/contexts/system_context.cpp
#   # add other .cpps as needed
# )
# # Make headers reachable with #include "..." from project root
# target_include_directories(tinykv_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# # Sanitizers toggle (you can turn off for release builds if you like)
# set(TEST_COMPILE_FLAGS "-O0;-g;-fsanitize=address,undefined")
# set(TEST_LINK_FLAGS    "-fsanitize=address,undefined")

# # -------- memtable_impl_test --------
# add_executable(memtable_impl_test
#   ./memtable_impl_test.cpp
# )
# target_link_libraries(memtable_impl_test
#   PRIVATE tinykv_core GTest::gmock_main
# )
# target_compile_options(memtable_impl_test PRIVATE ${TEST_COMPILE_FLAGS})
# target_link_options(memtable_impl_test    PRIVATE ${TEST_LINK_FLAGS})

# # -------- sstable_manager_impl_test (your new test) --------
# add_executable(sstable_manager_impl_test
#   ./unit_tests/sstable_file_manager_impl_test.cpp
# )
# target_link_libraries(sstable_manager_impl_test
#   PRIVATE tinykv_core GTest::gmock_main
# )
# target_compile_options(sstable_manager_impl_test PRIVATE ${TEST_COMPILE_FLAGS})
# target_link_options(sstable_manager_impl_test    PRIVATE ${TEST_LINK_FLAGS})

# # -------- gtest discovery --------
# include(GoogleTest)
# gtest_discover_tests(memtable_impl_test)
# gtest_discover_tests(sstable_manager_impl_test)
