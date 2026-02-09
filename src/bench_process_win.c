/**
 * @file bench_process_win.c
 * @brief Windows implementation of the multi-process benchmark.
 *
 * Uses CreateProcess() to spawn child workers and CreateFileMapping()
 * for shared memory. Since Windows has no fork(), the parent:
 * 1. Creates a named shared memory segment containing the dataset and
 *    result slots for each worker.
 * 2. Spawns N child processes (same executable with --worker flag).
 * 3. Waits for all children, then reads results from shared memory.
 *
 * Each child process (entered via cb_bench_process_worker_main):
 * 1. Opens the shared memory by name.
 * 2. Computes cb_array_sum() on its assigned slice.
 * 3. Writes cb_result_t to its designated result slot.
 * 4. Exits.
 *
 * Shared memory layout:
 *   [ int dataset[array_size] | cb_result_t results[num_workers] ]
 *
 * This file is only compiled on Windows targets.
 */

#ifndef _WIN32
#error "bench_process_win.c must not be compiled on non-Windows platforms"
#endif

#include "bench_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "input.h"
#include "platform.h"
#include "stats.h"
#include "worker.h"

/**
 * @brief Compute the total shared memory size for the dataset and results.
 *
 * @param array_size    Number of elements in the dataset.
 * @param num_workers   Number of worker processes.
 * @return Total size in bytes.
 */
static size_t shm_total_size(int array_size, int num_workers)
{
    return (size_t)array_size * sizeof(int) +
           (size_t)num_workers * sizeof(cb_result_t);
}

/**
 * @brief Get a pointer to the dataset within shared memory.
 * @param base  Base address of the shared memory region.
 * @return Pointer to the first element of the dataset.
 */
static int *shm_dataset(void *base)
{
    return (int *)base;
}

/**
 * @brief Get a pointer to a worker's result slot within shared memory.
 *
 * @param base        Base address of the shared memory region.
 * @param array_size  Number of elements in the dataset.
 * @param worker_id   Index of the worker (0-based).
 * @return Pointer to the worker's cb_result_t slot.
 */
static cb_result_t *shm_result(void *base, int array_size, int worker_id)
{
    uint8_t *p = (uint8_t *)base;
    p += (size_t)array_size * sizeof(int);
    return (cb_result_t *)p + worker_id;
}

cb_error_t cb_bench_process_run(const int *dataset,
                                const cb_config_t *config,
                                cb_run_report_t *report)
{
    cb_error_t err = CB_OK;
    cb_shared_mem_t shm;
    cb_process_t *procs = NULL;
    double *times = NULL;
    char shm_name[64];
    char exe_path[CB_MAX_PATH];
    int spawned = 0;
    bool shm_created = false;

    if (!dataset || !config || !report) {
        return CB_ERR_ARGS;
    }

    int n = config->num_processes;

    /* Get our own executable path for respawning. */
    err = cb_get_exe_path(exe_path, sizeof(exe_path));
    if (err) {
        return err;
    }

    procs = calloc((size_t)n, sizeof(cb_process_t));
    times = calloc((size_t)config->iterations, sizeof(double));
    if (!procs || !times) {
        err = CB_ERR_ALLOC;
        goto cleanup;
    }

    /* Generate a unique shared memory name. */
    snprintf(shm_name, sizeof(shm_name), "concur_bench_%lu",
             (unsigned long)GetCurrentProcessId());

    size_t total_size = shm_total_size(config->array_length, n);

    /* Create shared memory and copy dataset into it. */
    memset(&shm, 0, sizeof(shm));
    err = cb_shared_mem_create(&shm, shm_name, total_size);
    if (err) {
        goto cleanup;
    }
    shm_created = true;

    void *shm_base = cb_shared_mem_ptr(&shm);
    memcpy(shm_dataset(shm_base), dataset,
           (size_t)config->array_length * sizeof(int));

    long int verified_sum = 0;

    for (int iter = 0; iter < config->iterations; iter++) {
        double iter_start = cb_time_now();
        long int iter_sum = 0;
        spawned = 0;

        /* Clear result slots. */
        for (int i = 0; i < n; i++) {
            memset(shm_result(shm_base, config->array_length, i), 0,
                   sizeof(cb_result_t));
        }

        /* Distribute work and spawn children. */
        int base_len  = config->array_length / n;
        int remainder = config->array_length % n;
        int offset    = 0;

        for (int i = 0; i < n; i++) {
            int chunk = base_len + (i < remainder ? 1 : 0);

            char id_str[16], size_str[16], nw_str[16];
            char start_str[16], len_str[16];
            snprintf(id_str, sizeof(id_str), "%d", i);
            snprintf(size_str, sizeof(size_str), "%d", config->array_length);
            snprintf(nw_str, sizeof(nw_str), "%d", n);
            snprintf(start_str, sizeof(start_str), "%d", offset);
            snprintf(len_str, sizeof(len_str), "%d", chunk);

            const char *child_argv[] = {
                exe_path, "--worker", id_str, shm_name,
                size_str, nw_str, start_str, len_str, NULL
            };

            err = cb_process_spawn(&procs[i], child_argv, NULL, NULL);
            if (err) {
                goto cleanup_children;
            }

            spawned++;
            offset += chunk;
        }

        /* Wait for all children. */
        for (int i = 0; i < spawned; i++) {
            int status;
            cb_error_t wait_err = cb_process_wait(&procs[i], &status);
            if (wait_err) {
                if (!err) err = wait_err;
                continue;
            }
            if (status != 0) {
                fprintf(stderr, "  WARNING: worker %d exited with status %d\n",
                        i, status);
                if (!err) err = CB_ERR_PLATFORM;
            }
        }
        spawned = 0;

        if (err) {
            goto cleanup;
        }

        /* Read results from shared memory. */
        for (int i = 0; i < n; i++) {
            cb_result_t *r = shm_result(shm_base, config->array_length, i);
            iter_sum += r->sum;

            if (config->verbose) {
                fprintf(stdout, "  worker %d: sum=%ld (%.6fs)\n",
                        i, r->sum, r->elapsed_sec);
            }
        }

        double iter_end = cb_time_now();
        times[iter] = iter_end - iter_start;

        if (config->verbose) {
            fprintf(stdout, "  iteration %d/%d: total sum=%ld (%.6fs)\n",
                    iter + 1, config->iterations, iter_sum, times[iter]);
        }

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
    if (shm_created) {
        cb_shared_mem_destroy(&shm);
    }
    free(procs);
    free(times);
    return err;
}

int cb_bench_process_worker_main(const void *args)
{
    const cb_worker_args_t *wa = (const cb_worker_args_t *)args;

    size_t total_size = shm_total_size(wa->array_size, wa->num_workers);

    cb_shared_mem_t shm;
    memset(&shm, 0, sizeof(shm));
    cb_error_t err = cb_shared_mem_open(&shm, wa->shm_name, total_size);
    if (err) {
        fprintf(stderr, "worker %d: failed to open shared memory '%s'\n",
                wa->worker_id, wa->shm_name);
        return 1;
    }

    void *base = cb_shared_mem_ptr(&shm);
    const int *data = (const int *)shm_dataset(base);

    cb_result_t result = cb_array_sum(data, wa->start, wa->length);

    /* Write result to our slot. */
    cb_result_t *slot = shm_result(base, wa->array_size, wa->worker_id);
    *slot = result;

    cb_shared_mem_destroy(&shm);
    return 0;
}
