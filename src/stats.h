/**
 * @file stats.h
 * @brief Statistical computation utilities for concur-bench.
 *
 * Provides functions to compute descriptive statistics (minimum, maximum,
 * arithmetic mean, and sample standard deviation) over arrays of elapsed
 * time measurements from benchmark iterations.
 */

#ifndef CB_STATS_H
#define CB_STATS_H

#include "error.h"
#include "types.h"

/**
 * @brief Compute descriptive statistics over an array of elapsed times.
 *
 * Calculates min, max, arithmetic mean, and sample standard deviation
 * from the provided time measurements. For a single iteration
 * (count == 1), the standard deviation is reported as 0.0.
 *
 * @param times      Array of elapsed time values in seconds.
 * @param count      Number of elements in the times array. Must be >= 1.
 * @param stats_out  Output statistics structure, filled on success.
 * @return CB_OK on success, CB_ERR_ARGS if times is NULL or count < 1.
 */
cb_error_t cb_stats_compute(const double *times, int count,
                            cb_bench_stats_t *stats_out);

#endif /* CB_STATS_H */
