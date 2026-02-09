/**
 * @file bench_thread.h
 * @brief Multi-threaded benchmark mode for concur-bench.
 *
 * Runs the array summation using multiple threads, each computing a
 * disjoint slice of the dataset. Results are accumulated into a shared
 * structure protected by a single mutex.
 *
 * This module resolves the following bugs from the original code:
 * - Bug B: Thread parameters are heap-allocated and properly freed.
 * - Bug C: A single shared mutex is used (passed by pointer), not one
 *   embedded per-thread parameter.
 * - Bug E: The thread function returns NULL explicitly.
 * - Bug G: All shared state updates happen within one critical section.
 */

#ifndef CB_BENCH_THREAD_H
#define CB_BENCH_THREAD_H

#include "error.h"
#include "types.h"

/**
 * @brief Run the multi-threaded benchmark.
 *
 * Creates num_threads threads, each summing a disjoint slice of the
 * dataset. The work is distributed evenly: the first (array_length %
 * num_threads) threads receive one extra element each.
 *
 * A single shared mutex protects the result accumulator, ensuring
 * correct concurrent updates. Executes config->iterations runs and
 * computes timing statistics across all iterations.
 *
 * @param dataset  Pointer to the integer array.
 * @param config   Benchmark configuration (reads array_length, num_threads,
 *                 iterations, verbose).
 * @param report   Output report, filled with timing statistics and the sum.
 * @return CB_OK on success, CB_ERR_THREAD, CB_ERR_MUTEX, or CB_ERR_ALLOC
 *         on failure.
 */
cb_error_t cb_bench_thread_run(const int *dataset,
                               const cb_config_t *config,
                               cb_run_report_t *report);

#endif /* CB_BENCH_THREAD_H */
