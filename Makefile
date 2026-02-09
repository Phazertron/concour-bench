##
## @file Makefile
## @brief Unix convenience wrapper around CMake for concur-bench.
##
## Provides simple targets for building, running, and cleaning the
## project without needing to remember CMake commands.
##
## Usage:
##   make              Build in Release mode
##   make debug        Build in Debug mode
##   make clean        Remove build directory
##   make run          Build and run with default settings
##

BUILD_DIR  := build
BUILD_TYPE ?= Release
EXECUTABLE := $(BUILD_DIR)/bin/concur-bench

.PHONY: all debug clean run help

## Default target: build in Release mode.
all:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) .. && cmake --build .
	@echo ""
	@echo "Build complete: $(EXECUTABLE)"

## Build with debug symbols and no optimization.
debug:
	@$(MAKE) BUILD_TYPE=Debug all

## Remove the build directory entirely.
clean:
	@rm -rf $(BUILD_DIR)
	@echo "Build directory removed."

## Build and run the benchmark.
run: all
	@echo ""
	@./$(EXECUTABLE)

## Print available targets.
help:
	@echo "Available targets:"
	@echo "  make          Build in Release mode"
	@echo "  make debug    Build in Debug mode"
	@echo "  make clean    Remove build directory"
	@echo "  make run      Build and run"
	@echo "  make help     Show this message"
