/**
 * @file worker.c
 * @brief Implementation of core computation functions.
 *
 * Provides the array summation logic (cb_array_sum) used by all three
 * benchmark modes, and the thread entry-point function
 * (cb_array_sum_thread_fn) used specifically by the multi-threaded
 * benchmark.
 *
 * This file resolves the following bugs from the original code:
 * - Bug E: The thread function now explicitly returns NULL.
 * - Bug G: All shared state updates (sum, earliest_start, latest_end)
 *   are performed within a single critical section, eliminating the
 *   TOCTOU race where a value read outside the lock could become stale
 *   before the conditional write inside the lock.
 */

#include "worker.h"

#include <stdio.h>

cb_result_t cb_array_sum(const int *dataset, int start, int length)
{
    cb_result_t result;
    result.sum = 0;

    double t_start = cb_time_now();

    for (int i = start; i < start + length; i++) {
        result.sum += dataset[i];
    }

    double t_end = cb_time_now();
    result.elapsed_sec = t_end - t_start;

    return result;
}

void *cb_array_sum_thread_fn(void *arg)
{
    cb_thread_param_t *p = (cb_thread_param_t *)arg;

    /* Compute the partial sum locally (no locking needed). */
    double t_start = cb_time_now();

    long int partial_sum = 0;
    for (int i = p->start; i < p->start + p->length; i++) {
        partial_sum += p->dataset[i];
    }

    double t_end = cb_time_now();

    /*
     * Single critical section for ALL shared state updates.
     * Both the reads (for comparison) and the writes happen inside the
     * lock, preventing the TOCTOU race present in the original code
     * (Bug G fix).
     */
    cb_mutex_lock(p->mutex);

    p->shared->sum += partial_sum;

    if (p->shared->earliest_start < 0.0 ||
        t_start < p->shared->earliest_start) {
        p->shared->earliest_start = t_start;
    }

    if (t_end > p->shared->latest_end) {
        p->shared->latest_end = t_end;
    }

    cb_mutex_unlock(p->mutex);

    if (p->verbose) {
        fprintf(stdout, "  thread [%d..%d): sum=%ld (%.6fs)\n",
                p->start, p->start + p->length, partial_sum,
                t_end - t_start);
    }

    return NULL; /* Bug E fix: explicit return value. */
}
