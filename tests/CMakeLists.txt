file(GLOB TEST_SOURCES *.cpp)

# compile all tests into one executable `tinykv_tests`
# our test .cpp files provide test cases
# gtest_main provides `main()`
# all linked into one executable, `tinykv_tests
add_executable(tinykv_tests ${TEST_SOURCES})

target_link_libraries(tinykv_tests
    PRIVATE
      tinykv_core
      gtest
      gtest_main
)

# automatically discover tests so CTest treats each GoogleTest case as an indiv CTest entry
include(GoogleTest)
gtest_discover_tests(
    tinykv_tests
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 5
)
