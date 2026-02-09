/**
 * @file bench_single.h
 * @brief Single-threaded benchmark mode for concur-bench.
 *
 * Provides the baseline benchmark that sums the entire dataset using
 * a single thread of execution. The results from this mode serve as
 * the reference for computing speedup ratios of the parallel modes.
 */

#ifndef CB_BENCH_SINGLE_H
#define CB_BENCH_SINGLE_H

#include "error.h"
#include "types.h"

/**
 * @brief Run the single-threaded benchmark.
 *
 * Executes config->iterations runs of the full-array summation using
 * cb_array_sum(), collects elapsed times, and computes timing statistics.
 * The sum is verified for consistency across all iterations.
 *
 * @param dataset  Pointer to the integer array.
 * @param config   Benchmark configuration (reads array_length, iterations, verbose).
 * @param report   Output report, filled with timing statistics and the sum.
 * @return CB_OK on success, CB_ERR_ALLOC if internal allocation fails.
 */
cb_error_t cb_bench_single_run(const int *dataset,
                               const cb_config_t *config,
                               cb_run_report_t *report);

#endif /* CB_BENCH_SINGLE_H */
