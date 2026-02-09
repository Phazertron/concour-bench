##
## @file src/CMakeLists.txt
## @brief Source-level CMake configuration for concur-bench.
##
## Lists all source files, selects platform-specific implementations,
## and configures link dependencies.
##

## Platform-independent source files.
set(COMMON_SOURCES
    main.c
    error.c
    input.c
    dataset.c
    worker.c
    bench_single.c
    bench_thread.c
    stats.c
    output.c
)

## Platform-specific source files.
if(WIN32)
    list(APPEND COMMON_SOURCES
        platform_win.c
        bench_process_win.c
    )
else()
    list(APPEND COMMON_SOURCES
        platform_unix.c
        bench_process_unix.c
    )
endif()

## Define the executable target.
add_executable(concur-bench ${COMMON_SOURCES})

## Link required libraries.
if(WIN32)
    ## Windows: kernel32 is linked automatically by MSVC.
    ## No additional link libraries needed.
else()
    ## Unix: requires pthreads and math library (for sqrt in stats.c).
    find_package(Threads REQUIRED)
    target_link_libraries(concur-bench PRIVATE Threads::Threads m)
endif()

## Set output directory for the executable.
set_target_properties(concur-bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

## Install target.
install(TARGETS concur-bench RUNTIME DESTINATION bin)
