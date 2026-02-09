/**
 * @file bench_process_unix.c
 * @brief Unix implementation of the multi-process benchmark.
 *
 * Uses fork() to spawn child processes that inherit the dataset via
 * copy-on-write. Each child computes the sum of its assigned slice,
 * writes a cb_result_t back through a pipe, and exits. The parent
 * reads results from all pipes and waits for children to terminate.
 *
 * This is a simplified "fire-and-forget" protocol that replaces the
 * original's interactive "sum"/"end" message exchange. The remainder
 * is distributed evenly at spawn time (first R children get +1 element),
 * eliminating the need for a second-round dispatch. This simplification
 * removes the source of bugs A and F.
 *
 * This file is only compiled on Unix/Linux/macOS targets.
 */

#ifdef _WIN32
/* Prevent accidental compilation on Windows. */
#error "bench_process_unix.c must not be compiled on Windows"
#endif

#include "bench_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "platform.h"
#include "stats.h"
#include "worker.h"

/**
 * @brief Data passed to the child process function via cb_process_spawn().
 *
 * Contains everything the child needs: its dataset slice parameters
 * and the pipe to write results back to the parent.
 */
typedef struct {
    const int *dataset;  /**< Pointer to the full data array. */
    int        start;    /**< Starting index for this child's slice. */
    int        length;   /**< Number of elements in this child's slice. */
    cb_pipe_t *pipe;     /**< Pipe for sending results to parent. */
    bool       verbose;  /**< If true, print per-child details. */
} child_work_t;

/**
 * @brief Child process entry function.
 *
 * Computes the assigned array slice, writes the result to the pipe,
 * closes the pipe, and exits. This function never returns (calls _exit).
 *
 * @param arg  Pointer to a child_work_t structure.
 */
static void child_fn(void *arg)
{
    child_work_t *work = (child_work_t *)arg;

    cb_result_t result = cb_array_sum(work->dataset, work->start, work->length);

    if (work->verbose) {
        fprintf(stdout, "  process [%d..%d): sum=%ld (%.6fs)\n",
                work->start, work->start + work->length,
                result.sum, result.elapsed_sec);
    }

    /* Send result to parent. */
    cb_pipe_write(work->pipe, &result, sizeof(result));
    cb_pipe_close_write(work->pipe);
    cb_pipe_close_read(work->pipe);

    _exit(EXIT_SUCCESS);
}

cb_error_t cb_bench_process_run(const int *dataset,
                                const cb_config_t *config,
                                cb_run_report_t *report)
{
    cb_error_t err = CB_OK;
    cb_pipe_t *pipes     = NULL;
    cb_process_t *procs  = NULL;
    child_work_t *work   = NULL;
    double *times        = NULL;

    if (!dataset || !config || !report) {
        return CB_ERR_ARGS;
    }

    int n = config->num_processes;

    pipes = calloc((size_t)n, sizeof(cb_pipe_t));
    procs = calloc((size_t)n, sizeof(cb_process_t));
    work  = calloc((size_t)n, sizeof(child_work_t));
    times = calloc((size_t)config->iterations, sizeof(double));

    if (!pipes || !procs || !work || !times) {
        err = CB_ERR_ALLOC;
        goto cleanup;
    }

    long int verified_sum = 0;

    for (int iter = 0; iter < config->iterations; iter++) {
        double iter_start = cb_time_now();
        long int iter_sum = 0;
        int spawned = 0;

        /* Distribute work evenly: first (remainder) children get +1. */
        int base_len  = config->array_length / n;
        int remainder = config->array_length % n;
        int offset    = 0;

        for (int i = 0; i < n; i++) {
            int chunk = base_len + (i < remainder ? 1 : 0);

            err = cb_pipe_create(&pipes[i]);
            if (err) {
                goto cleanup_children;
            }

            work[i].dataset = dataset;
            work[i].start   = offset;
            work[i].length  = chunk;
            work[i].pipe    = &pipes[i];
            work[i].verbose = config->verbose;

            err = cb_process_spawn(&procs[i], NULL, child_fn, &work[i]);
            if (err) {
                cb_pipe_close_read(&pipes[i]);
                cb_pipe_close_write(&pipes[i]);
                goto cleanup_children;
            }

            /* Parent only reads from this pipe. */
            cb_pipe_close_write(&pipes[i]);
            spawned++;
            offset += chunk;
        }

        /* Read results from each child. */
        for (int i = 0; i < n; i++) {
            cb_result_t child_result;
            err = cb_pipe_read(&pipes[i], &child_result, sizeof(child_result));
            if (err) {
                goto cleanup_children;
            }
            cb_pipe_close_read(&pipes[i]);
            iter_sum += child_result.sum;
        }

        /* Wait for all children to exit. */
        for (int i = 0; i < spawned; i++) {
            int status;
            cb_error_t wait_err = cb_process_wait(&procs[i], &status);
            if (wait_err) {
                if (!err) err = wait_err;
                continue;
            }
            if (status != 0) {
                fprintf(stderr, "  WARNING: child process %d exited with "
                        "status %d\n",
                        cb_process_get_id(&procs[i]), status);
            }
        }
        spawned = 0;

        if (err) {
            goto cleanup;
        }

        double iter_end = cb_time_now();
        times[iter] = iter_end - iter_start;

        if (config->verbose) {
            fprintf(stdout, "  iteration %d/%d: total sum=%ld (%.6fs)\n",
                    iter + 1, config->iterations, iter_sum, times[iter]);
        }

        /* Verify sum consistency. */
        if (iter == 0) {
            verified_sum = iter_sum;
        } else if (iter_sum != verified_sum) {
            fprintf(stderr, "  WARNING: sum mismatch in process iteration %d "
                    "(expected %ld, got %ld)\n",
                    iter + 1, verified_sum, iter_sum);
        }

        continue;

    cleanup_children:
        for (int j = 0; j < spawned; j++) {
            cb_process_kill(&procs[j]);
            cb_process_wait(&procs[j], NULL);
        }
        goto cleanup;
    }

    report->label = "process";
    report->sum = verified_sum;
    report->parallelism = n;

    err = cb_stats_compute(times, config->iterations, &report->stats);

cleanup:
    free(pipes);
    free(procs);
    free(work);
    free(times);
    return err;
}
