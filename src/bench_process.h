/**
 * @file bench_process.h
 * @brief Multi-process benchmark mode for concur-bench.
 *
 * Declares the interface for the multi-process benchmark, which spawns
 * child processes to compute disjoint slices of the dataset in parallel.
 *
 * The implementation differs fundamentally between platforms:
 * - Unix (bench_process_unix.c): Uses fork() + pipes. Children inherit
 *   the dataset via copy-on-write and send results back through pipes.
 * - Windows (bench_process_win.c): Uses CreateProcess() + shared memory.
 *   The parent creates a named shared memory segment, spawns children
 *   with --worker arguments, and reads results from shared memory.
 *
 * Only one implementation is compiled per target (selected by CMake).
 *
 * This module resolves the following bugs from the original code:
 * - Bug A: The assignment-instead-of-comparison bug is eliminated by
 *   the redesigned fire-and-forget protocol (no wait loop).
 * - Bug F: The out-of-bounds array access is eliminated by the
 *   simplified architecture (no nProcess+1 indexing).
 */

#ifndef CB_BENCH_PROCESS_H
#define CB_BENCH_PROCESS_H

#include "error.h"
#include "types.h"

/**
 * @brief Run the multi-process benchmark.
 *
 * Spawns num_processes child processes, each computing a disjoint slice
 * of the dataset. Work is distributed evenly: the first (array_length %
 * num_processes) children receive one extra element each.
 *
 * Executes config->iterations runs and computes timing statistics.
 *
 * @param dataset  Pointer to the integer array.
 * @param config   Benchmark configuration (reads array_length, num_processes,
 *                 iterations, verbose).
 * @param report   Output report, filled with timing statistics and the sum.
 * @return CB_OK on success, or CB_ERR_FORK, CB_ERR_PIPE, CB_ERR_ALLOC,
 *         CB_ERR_SHM on failure.
 */
cb_error_t cb_bench_process_run(const int *dataset,
                                const cb_config_t *config,
                                cb_run_report_t *report);

#ifdef _WIN32
/**
 * @brief Entry point for a Windows worker child process.
 *
 * Called from main() when --worker is detected in argv. Opens the
 * named shared memory segment, computes the assigned array slice,
 * writes the result into the worker's result slot in shared memory,
 * and exits.
 *
 * @param args  Parsed worker arguments (worker_id, shm_name, etc.).
 * @return Exit code: 0 on success, 1 on failure.
 */
int cb_bench_process_worker_main(const void *args);
#endif

#endif /* CB_BENCH_PROCESS_H */
