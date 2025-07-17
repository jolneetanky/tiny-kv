cmake_minimum_required(VERSION 3.14)
project(tinykv)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Define the executable
add_executable(tinykv ${SOURCES})

# Optionally add include directories
target_include_directories(tinykv PRIVATE src)
