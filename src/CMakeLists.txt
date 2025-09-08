# Collect all .cpp files under src/, except main.cpp
file(GLOB_RECURSE TINYKV_SRC
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)
list(FILTER TINYKV_SRC EXCLUDE REGEX ".*/main\\.cpp$")

add_library(tinykv_core STATIC ${TINYKV_SRC})

# Public includes for consumers (tests, benchmarks)
target_include_directories(tinykv_core PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

add_executable(tinykv main.cpp)
target_link_libraries(tinykv PRIVATE tinykv_core)