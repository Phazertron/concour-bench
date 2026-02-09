# Contributing to concur-bench

Thank you for your interest in contributing. This document outlines the
guidelines and conventions to follow when submitting changes.

## Getting Started

1. Fork the repository and clone your fork.
2. Create a feature branch from `main`: `git checkout -b feature/your-feature`.
3. Make your changes following the conventions below.
4. Build and test on at least one platform (Unix or Windows).
5. Submit a pull request with a clear description of your changes.

## Build Instructions

### Unix (Linux / macOS)

```
make
```

Or manually:

```
mkdir build && cd build
cmake ..
cmake --build .
```

### Windows

```
mkdir build && cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

## Code Style

### Naming

- All public symbols use the `cb_` prefix.
- Functions and variables: `snake_case`.
- Types: `cb_<name>_t` (e.g., `cb_result_t`).
- Constants and macros: `CB_UPPER_SNAKE_CASE`.
- Static/internal functions: `snake_case` without the `cb_` prefix.
- Struct members: `snake_case`, no prefix.

### Formatting

- C11 standard, no compiler extensions.
- 4-space indentation, no tabs.
- Braces on their own line for function definitions.
- Braces on the same line for control flow (`if`, `for`, `while`, `switch`).
- Maximum line length: 100 characters (soft limit).

### Documentation

- Every public function, struct, enum, and constant requires a Doxygen
  documentation comment (`/** ... */`).
- File-level documentation blocks are required for every `.h` and `.c` file.
- Implementation details may use `/* ... */` or `//` inline comments
  sparingly and only where the logic is not self-evident.

### Error Handling

- All functions that can fail return `cb_error_t`.
- Use the `goto cleanup` pattern for resource management.
- Use `cb_perror()` for error reporting, not bare `printf`/`fprintf`.
- Initialize resource pointers to NULL at the top of functions.

### Platform Abstraction

- Never include platform-specific headers (`<windows.h>`, `<unistd.h>`,
  `<pthread.h>`) outside of `platform_unix.c` and `platform_win.c`.
- All platform-specific code goes through the `platform.h` API.
- Use `CB_PLATFORM_WINDOWS` and `CB_PLATFORM_UNIX` macros for
  compile-time platform checks.

## Adding a New Benchmark Mode

1. Create `src/bench_<name>.h` with the run function declaration.
2. Create `src/bench_<name>.c` (or platform-specific variants).
3. Add the source files to `src/CMakeLists.txt`.
4. Add a `cb_run_report_t` field to `cb_session_t` in `types.h`.
5. Call the new benchmark from `main.c`.
6. Update `output.c` to display the new mode in the results table.

## Reporting Issues

Please include:
- Operating system and version.
- Compiler and version.
- CMake version.
- Steps to reproduce the issue.
- Expected vs actual behavior.
