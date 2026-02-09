/**
 * @file worker.h
 * @brief Core computation functions and thread parameter types.
 *
 * Contains the fundamental array summation logic used by all benchmark
 * modes, along with the data structures needed for the multi-threaded
 * benchmark's thread entry point.
 *
 * Design notes on bug fixes from the original code:
 * - Bug C fix: The mutex is a POINTER to a single shared mutex, not
 *   embedded per-thread. All threads lock the same properly initialized
 *   mutex instance.
 * - Bug E fix: The thread function returns NULL explicitly.
 * - Bug G fix: All reads and writes to shared state (sum, timestamps)
 *   occur within a single critical section, eliminating the TOCTOU race.
 */

#ifndef CB_WORKER_H
#define CB_WORKER_H

#include <stdbool.h>

#include "platform.h"
#include "types.h"

/**
 * @brief Shared accumulator state for the multi-threaded benchmark.
 *
 * All fields are protected by the shared mutex. Threads must hold the
 * mutex for the ENTIRE duration of reading and conditionally writing
 * these fields, preventing time-of-check/time-of-use races.
 */
typedef struct {
    long int sum;             /**< Running total sum across all threads. */
    double   earliest_start;  /**< Minimum start time seen (-1.0 = unset). */
    double   latest_end;      /**< Maximum end time seen. */
} cb_thread_shared_t;

/**
 * @brief Parameters passed to each thread in the multi-threaded benchmark.
 *
 * Each thread receives its own cb_thread_param_t with distinct start/length
 * values, but all threads share the same mutex and shared accumulator
 * via pointers.
 */
typedef struct {
    const int            *dataset;  /**< Pointer to the full data array (read-only). */
    int                   start;    /**< Starting index for this thread's slice. */
    int                   length;   /**< Number of elements in this thread's slice. */
    cb_thread_shared_t   *shared;   /**< Pointer to the single shared accumulator. */
    cb_mutex_t           *mutex;    /**< Pointer to the single shared mutex. */
    bool                  verbose;  /**< If true, print per-thread details. */
} cb_thread_param_t;

/**
 * @brief Compute the sum of a contiguous slice of an integer array.
 *
 * This is the core computation function used by all benchmark modes.
 * It performs pure computation with no side effects: no I/O, no locking,
 * no global state access. Thread-safe by design (operates only on
 * the provided parameters).
 *
 * @param dataset  Pointer to the full integer array.
 * @param start    Starting index (inclusive).
 * @param length   Number of elements to sum starting from @p start.
 * @return A cb_result_t containing the sum and the wall-clock elapsed time.
 */
cb_result_t cb_array_sum(const int *dataset, int start, int length);

/**
 * @brief Thread entry-point function for the multi-threaded benchmark.
 *
 * Computes the partial sum of this thread's assigned slice, then
 * acquires the shared mutex to atomically update the shared accumulator
 * (sum and timestamp tracking). All shared state reads AND writes occur
 * within a single critical section.
 *
 * @param arg  Pointer to a cb_thread_param_t structure.
 * @return NULL always.
 */
void *cb_array_sum_thread_fn(void *arg);

#endif /* CB_WORKER_H */
