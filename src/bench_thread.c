/**
 * @file bench_thread.c
 * @brief Implementation of the multi-threaded benchmark mode.
 *
 * Creates N threads via the platform abstraction, distributes array
 * slices evenly (including remainder), joins all threads, and collects
 * timing results. Uses a single shared mutex to protect the result
 * accumulator.
 *
 * Bug fixes incorporated:
 * - Bug B: params array is freed in the cleanup path.
 * - Bug C: One mutex initialized, all threads receive a pointer to it.
 * - Bug E: Thread function returns NULL (handled in worker.c).
 * - Bug G: TOCTOU race fixed (handled in worker.c).
 */

#include "bench_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "stats.h"
#include "worker.h"

cb_error_t cb_bench_thread_run(const int *dataset,
                               const cb_config_t *config,
                               cb_run_report_t *report)
{
    cb_error_t err = CB_OK;
    cb_thread_t *threads = NULL;
    cb_thread_param_t *params = NULL;
    double *times = NULL;
    bool mutex_initialized = false;
    cb_mutex_t shared_mutex;

    if (!dataset || !config || !report) {
        return CB_ERR_ARGS;
    }

    int n = config->num_threads;

    /* Allocate arrays for thread handles, parameters, and iteration times. */
    threads = calloc((size_t)n, sizeof(cb_thread_t));
    params  = calloc((size_t)n, sizeof(cb_thread_param_t));
    times   = calloc((size_t)config->iterations, sizeof(double));

    if (!threads || !params || !times) {
        err = CB_ERR_ALLOC;
        goto cleanup;
    }

    /*
     * Initialize a SINGLE shared mutex. All threads receive a pointer
     * to this mutex, not their own copy (Bug C fix).
     */
    err = cb_mutex_init(&shared_mutex);
    if (err) {
        goto cleanup;
    }
    mutex_initialized = true;

    long int verified_sum = 0;

    for (int iter = 0; iter < config->iterations; iter++) {
        /* Reset shared accumulator for this iteration. */
        cb_thread_shared_t shared = {
            .sum = 0,
            .earliest_start = -1.0,
            .latest_end = 0.0
        };

        /* Distribute work: first (remainder) threads get one extra element. */
        int base_len  = config->array_length / n;
        int remainder = config->array_length % n;
        int offset    = 0;

        for (int i = 0; i < n; i++) {
            int chunk = base_len + (i < remainder ? 1 : 0);

            params[i].dataset = dataset;
            params[i].start   = offset;
            params[i].length  = chunk;
            params[i].shared  = &shared;
            params[i].mutex   = &shared_mutex;
            params[i].verbose = config->verbose;

            offset += chunk;
        }

        /* Create all threads. */
        int created = 0;
        for (int i = 0; i < n; i++) {
            err = cb_thread_create(&threads[i],
                                   cb_array_sum_thread_fn,
                                   &params[i]);
            if (err) {
                /* Join already-created threads before reporting error. */
                for (int j = 0; j < created; j++) {
                    cb_thread_join(&threads[j]);
                }
                goto cleanup;
            }
            created++;
        }

        /* Join all threads. */
        for (int i = 0; i < n; i++) {
            cb_error_t join_err = cb_thread_join(&threads[i]);
            if (join_err && !err) {
                err = join_err;
            }
        }

        if (err) {
            goto cleanup;
        }

        /* Record iteration timing. */
        double elapsed = shared.latest_end - shared.earliest_start;
        times[iter] = elapsed;

        if (config->verbose) {
            fprintf(stdout, "  iteration %d/%d: total sum=%ld (%.6fs)\n",
                    iter + 1, config->iterations, shared.sum, elapsed);
        }

        /* Verify sum consistency. */
        if (iter == 0) {
            verified_sum = shared.sum;
        } else if (shared.sum != verified_sum) {
            fprintf(stderr, "  WARNING: sum mismatch in thread iteration %d "
                    "(expected %ld, got %ld)\n",
                    iter + 1, verified_sum, shared.sum);
        }
    }

    report->label = "thread";
    report->sum = verified_sum;
    report->parallelism = n;

    err = cb_stats_compute(times, config->iterations, &report->stats);

cleanup:
    if (mutex_initialized) {
        cb_mutex_destroy(&shared_mutex);
    }
    free(threads);  /* Bug B fix: params are freed. */
    free(params);
    free(times);
    return err;
}
