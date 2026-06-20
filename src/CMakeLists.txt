# Collect all .cpp files under src/, except executable entrypoints
file(GLOB_RECURSE TINYKV_SRC
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

# Exclude executable entrypoints from core library sources
list(FILTER TINYKV_SRC EXCLUDE REGEX ".*/server_main\\.cpp$")
list(FILTER TINYKV_SRC EXCLUDE REGEX ".*/main\\.cpp$")
list(FILTER TINYKV_SRC EXCLUDE REGEX ".*/cli_main\\.cpp$")

# Build tinykv_core library
add_library(tinykv_core STATIC ${TINYKV_SRC})

# “Anyone who links against tinykv_core inherits its include paths.”
# Means when compiling this target, we tell the compiler to add this directory to the header search path.
target_include_directories(tinykv_core PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

if (TINYKV_ENABLE_LOGGING)
  target_compile_definitions(tinykv_core PUBLIC TINYKV_ENABLE_LOGGING=1)
else()
  target_compile_definitions(tinykv_core PUBLIC TINYKV_ENABLE_LOGGING=0)
endif()

# CLI executable
add_executable(tinykv_cli
  ${CMAKE_CURRENT_LIST_DIR}/cli_main.cpp
)
target_link_libraries(tinykv_cli
  PRIVATE
    tinykv_core
)

# TCP server executable (session + connection skeleton)
add_executable(tinykv_server
  ${CMAKE_CURRENT_LIST_DIR}/server_main.cpp
)
target_link_libraries(tinykv_server
  PRIVATE
    tinykv_core
)
