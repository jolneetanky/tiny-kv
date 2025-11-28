file(GLOB TEST_SOURCES *.cpp)

add_executable(tinykv_tests ${TEST_SOURCES})

target_link_libraries(tinykv_tests
    PRIVATE
      tinykv_core
      gtest
      gtest_main
)

# target_include_directories(tinykv_tests PRIVATE
#     ${gtest_SOURCE_DIR}/include
# )
target_link_libraries(tinykv_tests
    PRIVATE tinykv_core gtest gtest_main
)

# automatically discover tests so CTest treats each GoogleTest case as an indiv CTest entry
include(GoogleTest)
gtest_discover_tests(tinykv_tests)

# compiles all test files into `tinykv_tests`
# links GTest + tinykv_core
# registers the test as `TinyKVTests`
# add_test(NAME TinyKVTests COMMAND tinykv_tests)