/**
 * @file output.h
 * @brief Result formatting and file output for concur-bench.
 *
 * Provides functions to display benchmark results on the terminal in
 * a formatted table, write detailed text reports, and export data in
 * CSV format for external analysis tools. Also handles creation of
 * the timestamped output directory structure.
 */

#ifndef CB_OUTPUT_H
#define CB_OUTPUT_H

#include <stddef.h>

#include "error.h"
#include "types.h"

/**
 * @brief Print a formatted results table to stdout.
 *
 * Displays a bordered ASCII table with columns for each benchmark mode
 * (single, process, thread), showing worker count, min/mean/max/stddev
 * timing, and speedup relative to the single-threaded baseline.
 *
 * Also prints a configuration summary, system information, and a
 * correctness verification note (whether all three modes produced
 * the same sum).
 *
 * @param session  Complete benchmark session results.
 */
void cb_output_terminal(const cb_session_t *session);

/**
 * @brief Create the timestamped run directory for output files.
 *
 * Creates the directory: <base_dir>/run_<timestamp>/
 * For example: results/run_20260209_143022/
 *
 * @param base_dir   Base directory for results (e.g., "results").
 * @param timestamp  Timestamp string in "YYYYMMDD_HHMMSS" format.
 * @param path_out   Output buffer filled with the full directory path.
 * @param path_size  Size of the path_out buffer in bytes.
 * @return CB_OK on success, CB_ERR_IO if directory creation fails.
 */
cb_error_t cb_output_create_run_dir(const char *base_dir,
                                    const char *timestamp,
                                    char *path_out,
                                    size_t path_size);

/**
 * @brief Write a detailed text report file.
 *
 * Creates "report.txt" in the specified directory, containing:
 * system information, full configuration dump, the results table,
 * correctness verification, and speedup analysis.
 *
 * @param session   Complete benchmark session results.
 * @param dir_path  Directory to write the report file into.
 * @return CB_OK on success, CB_ERR_IO if file creation fails.
 */
cb_error_t cb_output_txt_report(const cb_session_t *session,
                                const char *dir_path);

/**
 * @brief Write a CSV file for machine-readable analysis.
 *
 * Creates "results.csv" in the specified directory with columns:
 * mode, workers, iterations, min_sec, mean_sec, max_sec, stddev_sec,
 * sum, speedup, array_length, seed
 *
 * @param session   Complete benchmark session results.
 * @param dir_path  Directory to write the CSV file into.
 * @return CB_OK on success, CB_ERR_IO if file creation fails.
 */
cb_error_t cb_output_csv(const cb_session_t *session,
                         const char *dir_path);

/**
 * @brief Generate the current timestamp in "YYYYMMDD_HHMMSS" format.
 *
 * Uses the local time zone.
 *
 * @param buf       Output buffer (must be at least 16 bytes).
 * @param buf_size  Size of the output buffer.
 */
void cb_output_timestamp(char *buf, size_t buf_size);

#endif /* CB_OUTPUT_H */
