##
## @file CMakeLists.txt
## @brief Top-level CMake build configuration for concur-bench.
##
## Supports cross-platform builds on Unix (GCC/Clang) and Windows (MSVC).
## Automatically selects the correct platform-specific source files.
##
## Usage:
##   mkdir build && cd build
##   cmake ..                          # Unix default
##   cmake -G "Visual Studio 17 2022" ..  # Windows Visual Studio
##   cmake --build .
##

cmake_minimum_required(VERSION 3.10)

project(concur-bench
    VERSION 1.0.0
    DESCRIPTION "Concurrency benchmark: single-thread vs multi-process vs multi-thread"
    LANGUAGES C
)

## Enforce C11 standard without compiler extensions.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

## On Unix with strict C11 (extensions OFF), POSIX symbols like
## clock_gettime, kill, readlink, etc. are hidden unless we explicitly
## request them via the _POSIX_C_SOURCE feature-test macro.
if(NOT WIN32)
    add_definitions(-D_POSIX_C_SOURCE=200809L)
endif()

## Compiler warnings.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror=implicit-function-declaration)
elseif(MSVC)
    add_compile_options(/W4)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(src)
