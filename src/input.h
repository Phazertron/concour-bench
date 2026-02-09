/**
 * @file input.h
 * @brief User input and argument parsing for concur-bench.
 *
 * Provides two input mechanisms:
 * 1. Command-line argument parsing for non-interactive configuration
 *    and the --worker flag used by the Windows process benchmark.
 * 2. Interactive prompts using fgets + strtol for robust input handling,
 *    replacing the fragile scanf-based approach from the original code.
 */

#ifndef CB_INPUT_H
#define CB_INPUT_H

#include <stdbool.h>

#include "error.h"
#include "types.h"

/**
 * @brief Worker arguments parsed from --worker command-line flag.
 *
 * Used exclusively on Windows when the executable is re-invoked as a
 * child process by the multi-process benchmark. Contains all the
 * information the child needs to perform its computation slice.
 */
typedef struct {
    int          worker_id;    /**< Numeric identifier for this worker. */
    char         shm_name[64]; /**< Name of the shared memory segment. */
    int          array_size;   /**< Total number of elements in the dataset. */
    int          num_workers;  /**< Total number of worker processes. */
    int          start;        /**< Starting index for this worker's slice. */
    int          length;       /**< Number of elements in this worker's slice. */
} cb_worker_args_t;

/**
 * @brief Parse command-line arguments into a configuration struct.
 *
 * Recognized flags:
 *   --worker <id> <shm_name> <array_size> <num_workers> <start> <length>
 *       Activate worker (child process) mode. Fills worker_args.
 *   --iterations <N>
 *       Set the number of benchmark iterations (default: CB_DEFAULT_ITERATIONS).
 *   --verbose
 *       Enable detailed per-worker output.
 *   --help
 *       Print usage information and return CB_ERR_ARGS to signal exit.
 *
 * Unrecognized flags are reported as errors.
 *
 * @param argc         Argument count from main().
 * @param argv         Argument vector from main().
 * @param config       Output configuration struct (partially filled).
 * @param is_worker    Output flag: set to true if --worker was specified.
 * @param worker_args  Output worker arguments (filled only if is_worker is true).
 * @return CB_OK on success, CB_ERR_ARGS on unrecognized or malformed arguments.
 */
cb_error_t cb_parse_args(int argc, char *argv[],
                         cb_config_t *config,
                         bool *is_worker,
                         cb_worker_args_t *worker_args);

/**
 * @brief Collect benchmark configuration interactively from the user.
 *
 * Prompts for: verbose mode (y/n), array length, number of processes,
 * number of threads, seed, and number of iterations. Uses fgets for
 * line input and strtol for numeric parsing, providing robust error
 * handling and retry logic.
 *
 * The detected CPU core count is displayed as a suggestion for the
 * process and thread count prompts.
 *
 * Fields already set by cb_parse_args() (e.g., iterations, verbose)
 * are used as defaults and may be overridden by the user.
 *
 * @param config  Configuration struct to fill. May have some fields
 *                pre-populated from command-line arguments.
 * @return CB_OK on success, CB_ERR_INPUT if stdin reaches EOF.
 */
cb_error_t cb_input_interactive(cb_config_t *config);

#endif /* CB_INPUT_H */
