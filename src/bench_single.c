/**
 * @file bench_single.c
 * @brief Implementation of the single-threaded benchmark mode.
 *
 * Runs the full-array summation repeatedly for the configured number
 * of iterations, collecting elapsed times and computing descriptive
 * statistics. This is the simplest benchmark mode and serves as the
 * baseline for speedup comparisons.
 */

#include "bench_single.h"

#include <stdio.h>
#include <stdlib.h>

#include "stats.h"
#include "worker.h"

cb_error_t cb_bench_single_run(const int *dataset,
                               const cb_config_t *config,
                               cb_run_report_t *report)
{
    cb_error_t err = CB_OK;
    double *times = NULL;

    if (!dataset || !config || !report) {
        return CB_ERR_ARGS;
    }

    times = calloc((size_t)config->iterations, sizeof(double));
    if (!times) {
        return CB_ERR_ALLOC;
    }

    long int verified_sum = 0;

    for (int iter = 0; iter < config->iterations; iter++) {
        cb_result_t result = cb_array_sum(dataset, 0, config->array_length);
        times[iter] = result.elapsed_sec;

        if (config->verbose) {
            fprintf(stdout, "  iteration %d/%d: sum=%ld (%.6fs)\n",
                    iter + 1, config->iterations,
                    result.sum, result.elapsed_sec);
        }

        /* Verify sum consistency across iterations. */
        if (iter == 0) {
            verified_sum = result.sum;
        } else if (result.sum != verified_sum) {
            fprintf(stderr, "  WARNING: sum mismatch in iteration %d "
                    "(expected %ld, got %ld)\n",
                    iter + 1, verified_sum, result.sum);
        }
    }

    report->label = "single";
    report->sum = verified_sum;
    report->parallelism = 1;

    err = cb_stats_compute(times, config->iterations, &report->stats);

    free(times);
    return err;
}
