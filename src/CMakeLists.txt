# Collect all .cpp files under src/, except cli.cpp and main.cpp
file(GLOB_RECURSE TINYKV_SRC
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

# Exclude cli.cpp and main.cpp from core library sources
list(FILTER TINYKV_SRC EXCLUDE REGEX ".*/cli\\.cpp$")
list(FILTER TINYKV_SRC EXCLUDE REGEX ".*/main\\.cpp$")

# Build tinykv_core library
add_library(tinykv_core STATIC ${TINYKV_SRC})

# “Anyone who links against tinykv_core inherits its include paths.”
# Means when compiling this target, we tell the compiler to add this directory to the header search path.
target_include_directories(tinykv_core PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

# disable for benchmarks, enable when u want
target_compile_definitions(tinykv_core
  PUBLIC TINYKV_ENABLE_LOGGING=1
)

# link spdlog for logging
target_link_libraries(tinykv_core
  PUBLIC
    spdlog::spdlog
)