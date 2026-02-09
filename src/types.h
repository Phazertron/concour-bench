/**
 * @file types.h
 * @brief Shared type definitions for concur-bench.
 *
 * Defines the core data structures used throughout the application:
 * benchmark configuration, computation results, statistical summaries,
 * per-mode reports, and the top-level session container. These types
 * form the data contract between all modules.
 */

#ifndef CB_TYPES_H
#define CB_TYPES_H

#include <stdbool.h>
#include <stddef.h>

/* ---- Constants ---- */

/** @brief Default minimum number of processes or threads. */
#define CB_MIN_WORKERS     1

/** @brief Default maximum number of processes or threads. */
#define CB_MAX_WORKERS     256

/** @brief Minimum dataset size (number of array elements). */
#define CB_MIN_ARRAY_LEN   1000

/** @brief Default number of benchmark iterations per mode. */
#define CB_DEFAULT_ITERATIONS  5

/* ---- Core Data Structures ---- */

/**
 * @brief Result of a single summation computation.
 *
 * Returned by the core worker function after summing a contiguous
 * slice of the dataset. Contains both the computed value and the
 * wall-clock time taken.
 */
typedef struct {
    long int sum;          /**< Computed summation value. */
    double   elapsed_sec;  /**< Wall-clock time for this computation (seconds). */
} cb_result_t;

/**
 * @brief Statistical summary across multiple benchmark iterations.
 *
 * Produced by cb_stats_compute() from an array of elapsed time
 * measurements. Provides the key descriptive statistics needed
 * for meaningful benchmark reporting.
 */
typedef struct {
    double min_sec;     /**< Minimum elapsed time across iterations. */
    double max_sec;     /**< Maximum elapsed time across iterations. */
    double mean_sec;    /**< Arithmetic mean of elapsed times. */
    double stddev_sec;  /**< Sample standard deviation of elapsed times. */
    int    iterations;  /**< Number of iterations performed. */
} cb_bench_stats_t;

/**
 * @brief Complete report for one benchmark mode (single / process / thread).
 *
 * Combines the final summation result, the degree of parallelism used,
 * and the timing statistics into a single structure that the output
 * module can format and display.
 */
typedef struct {
    const char       *label;       /**< Mode identifier: "single", "process", or "thread". */
    long int          sum;         /**< Final summation result (used for correctness check). */
    int               parallelism; /**< Number of workers (1 for single-threaded). */
    cb_bench_stats_t  stats;       /**< Timing statistics across all iterations. */
} cb_run_report_t;

/**
 * @brief Full benchmark configuration, populated from user input.
 *
 * Filled by the input module (either from command-line arguments or
 * interactive prompts) and passed to every benchmark module to control
 * execution parameters.
 */
typedef struct {
    int          array_length;  /**< Number of elements in the dataset. */
    int          num_processes; /**< Number of child processes for the process benchmark. */
    int          num_threads;   /**< Number of threads for the thread benchmark. */
    unsigned int seed;          /**< RNG seed (0 = generate from current time). */
    int          iterations;    /**< Number of benchmark iterations per mode. */
    bool         verbose;       /**< Enable detailed per-worker output. */
} cb_config_t;

/**
 * @brief Complete benchmark session results.
 *
 * Top-level container holding the configuration, all three benchmark
 * reports, system information, and a timestamp. Passed to the output
 * module to generate the terminal display, text report, and CSV file.
 */
typedef struct {
    cb_config_t     config;            /**< Configuration used for this run. */
    cb_run_report_t single;            /**< Single-threaded benchmark results. */
    cb_run_report_t process;           /**< Multi-process benchmark results. */
    cb_run_report_t thread;            /**< Multi-threaded benchmark results. */
    char            system_info[256];  /**< OS and CPU description string. */
    char            timestamp[32];     /**< Run timestamp in "YYYYMMDD_HHMMSS" format. */
} cb_session_t;

#endif /* CB_TYPES_H */
