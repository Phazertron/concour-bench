/**
 * @file dataset.h
 * @brief Dataset generation and management for concur-bench.
 *
 * Provides functions to allocate and populate the integer array used
 * as input to all benchmark modes. The array is filled with pseudo-random
 * values in [1, 100] using the configured seed for reproducibility.
 */

#ifndef CB_DATASET_H
#define CB_DATASET_H

#include <stdbool.h>

#include "error.h"
#include "types.h"

/**
 * @brief Allocate and populate a random integer array.
 *
 * If config->seed is 0, a time-based seed is generated and written
 * back to config->seed so it can be reported in the output. When a
 * nonzero seed is provided, srand() is called with that exact value
 * to ensure reproducible datasets.
 *
 * Each element is set to a uniformly distributed integer in [1, 100].
 *
 * When verbose is true, a progress indicator is printed to stdout
 * (one '+' character per 10% of the array populated).
 *
 * @param config    Benchmark configuration. Reads array_length and seed.
 *                  May write seed if the original value was 0.
 * @param data_out  Output pointer to the allocated array. On success,
 *                  the caller is responsible for freeing this memory
 *                  via cb_dataset_destroy().
 * @param verbose   If true, print a progress indicator during population.
 * @return CB_OK on success, CB_ERR_ALLOC if memory allocation fails.
 */
cb_error_t cb_dataset_create(cb_config_t *config, int **data_out,
                             bool verbose);

/**
 * @brief Free a dataset array previously allocated by cb_dataset_create().
 *
 * Safe to call with a NULL pointer (no-op in that case).
 *
 * @param data  Pointer to the array to free, or NULL.
 */
void cb_dataset_destroy(int *data);

#endif /* CB_DATASET_H */
