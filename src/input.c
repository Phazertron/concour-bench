/**
 * @file input.c
 * @brief Implementation of user input and argument parsing.
 *
 * Replaces the original scanf-based input with fgets + strtol for
 * robust error handling. Fixes Bug D (operator precedence in the
 * original scanf condition) by eliminating scanf entirely.
 */

#include "input.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

/** @brief Maximum length of a single input line. */
#define INPUT_BUF_SIZE 256

/**
 * @brief Read a long integer from stdin with prompt, validation, and retry.
 *
 * Displays the prompt, reads a line with fgets, parses with strtol,
 * and validates the result against the provided range. On invalid input,
 * prints an error message and re-prompts. Returns CB_ERR_INPUT only
 * if stdin reaches EOF.
 *
 * @param prompt   Prompt string displayed before reading input.
 * @param min_val  Minimum acceptable value (inclusive).
 * @param max_val  Maximum acceptable value (inclusive).
 * @param out      Output: the parsed and validated value.
 * @return CB_OK on success, CB_ERR_INPUT on EOF.
 */
static cb_error_t read_long(const char *prompt, long min_val,
                            long max_val, long *out)
{
    char buf[INPUT_BUF_SIZE];

    while (1) {
        fprintf(stdout, "%s", prompt);
        fflush(stdout);

        if (!fgets(buf, sizeof(buf), stdin)) {
            return CB_ERR_INPUT; /* EOF or read error. */
        }

        /* Strip trailing newline. */
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }

        errno = 0;
        char *endptr;
        long val = strtol(buf, &endptr, 10);

        /* Check for parse errors. */
        if (endptr == buf) {
            fprintf(stdout, "  Invalid input: not a number. Try again.\n");
            continue;
        }

        if (*endptr != '\0') {
            fprintf(stdout, "  Invalid input: trailing characters. Try again.\n");
            continue;
        }

        if (errno == ERANGE) {
            fprintf(stdout, "  Invalid input: number out of representable range. Try again.\n");
            continue;
        }

        if (val < min_val || val > max_val) {
            fprintf(stdout, "  Invalid input: must be between %ld and %ld. Try again.\n",
                    min_val, max_val);
            continue;
        }

        *out = val;
        return CB_OK;
    }
}

/**
 * @brief Read an unsigned long integer from stdin with prompt and validation.
 *
 * Uses strtoul instead of strtol to correctly handle the full unsigned range.
 * Required for the seed input where the valid range is [0, UINT_MAX], and
 * UINT_MAX exceeds the signed long range on Windows (where long is 32-bit).
 *
 * @param prompt   Prompt string displayed before reading input.
 * @param min_val  Minimum acceptable value (inclusive).
 * @param max_val  Maximum acceptable value (inclusive).
 * @param out      Output: the parsed and validated value.
 * @return CB_OK on success, CB_ERR_INPUT on EOF.
 */
static cb_error_t read_ulong(const char *prompt, unsigned long min_val,
                             unsigned long max_val, unsigned long *out)
{
    char buf[INPUT_BUF_SIZE];

    while (1) {
        fprintf(stdout, "%s", prompt);
        fflush(stdout);

        if (!fgets(buf, sizeof(buf), stdin)) {
            return CB_ERR_INPUT;
        }

        /* Strip trailing newline. */
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }

        /* Reject explicitly negative input (strtoul silently wraps). */
        if (buf[0] == '-') {
            fprintf(stdout, "  Invalid input: must be a non-negative number. Try again.\n");
            continue;
        }

        errno = 0;
        char *endptr;
        unsigned long val = strtoul(buf, &endptr, 10);

        if (endptr == buf) {
            fprintf(stdout, "  Invalid input: not a number. Try again.\n");
            continue;
        }

        if (*endptr != '\0') {
            fprintf(stdout, "  Invalid input: trailing characters. Try again.\n");
            continue;
        }

        if (errno == ERANGE) {
            fprintf(stdout, "  Invalid input: number out of representable range. Try again.\n");
            continue;
        }

        if (val < min_val || val > max_val) {
            fprintf(stdout, "  Invalid input: must be between %lu and %lu. Try again.\n",
                    min_val, max_val);
            continue;
        }

        *out = val;
        return CB_OK;
    }
}

/**
 * @brief Read a yes/no answer from stdin.
 *
 * Accepts 'y', 'Y', 'n', 'N' (first character of the input line).
 * Re-prompts on invalid input. Returns CB_ERR_INPUT on EOF.
 *
 * @param prompt  Prompt string displayed before reading input.
 * @param out     Output: true for yes, false for no.
 * @return CB_OK on success, CB_ERR_INPUT on EOF.
 */
static cb_error_t read_yes_no(const char *prompt, bool *out)
{
    char buf[INPUT_BUF_SIZE];

    while (1) {
        fprintf(stdout, "%s", prompt);
        fflush(stdout);

        if (!fgets(buf, sizeof(buf), stdin)) {
            return CB_ERR_INPUT;
        }

        if (buf[0] == 'y' || buf[0] == 'Y') {
            *out = true;
            return CB_OK;
        }

        if (buf[0] == 'n' || buf[0] == 'N') {
            *out = false;
            return CB_OK;
        }

        fprintf(stdout, "  Invalid input: please enter 'y' or 'n'.\n");
    }
}

/**
 * @brief Print usage information to stdout.
 */
static void print_usage(const char *prog_name)
{
    fprintf(stdout,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  --verbose            Enable detailed per-worker output\n"
        "  --iterations <N>     Number of benchmark iterations (default: %d)\n"
        "  --help               Show this help message and exit\n"
        "\n"
        "When run without options, the program prompts interactively\n"
        "for all configuration parameters.\n",
        prog_name ? prog_name : "concur-bench",
        CB_DEFAULT_ITERATIONS);
}

cb_error_t cb_parse_args(int argc, char *argv[],
                         cb_config_t *config,
                         bool *is_worker,
                         cb_worker_args_t *worker_args)
{
    if (!config || !is_worker || !worker_args) {
        return CB_ERR_ARGS;
    }

    /* Initialize defaults. */
    memset(config, 0, sizeof(*config));
    config->iterations = CB_DEFAULT_ITERATIONS;
    config->verbose = false;
    *is_worker = false;
    memset(worker_args, 0, sizeof(*worker_args));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return CB_ERR_ARGS; /* Signal caller to exit cleanly. */
        }

        if (strcmp(argv[i], "--verbose") == 0) {
            config->verbose = true;
            continue;
        }

        if (strcmp(argv[i], "--iterations") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "concur-bench: --iterations requires a value\n");
                return CB_ERR_ARGS;
            }
            i++;
            errno = 0;
            char *endptr;
            long val = strtol(argv[i], &endptr, 10);
            if (endptr == argv[i] || *endptr != '\0' || errno == ERANGE ||
                val < 1 || val > 1000) {
                fprintf(stderr, "concur-bench: invalid iteration count: %s\n",
                        argv[i]);
                return CB_ERR_ARGS;
            }
            config->iterations = (int)val;
            continue;
        }

        if (strcmp(argv[i], "--worker") == 0) {
            /* --worker <id> <shm_name> <array_size> <num_workers> <start> <length> */
            if (i + 6 >= argc) {
                fprintf(stderr, "concur-bench: --worker requires 6 arguments\n");
                return CB_ERR_ARGS;
            }
            *is_worker = true;
            worker_args->worker_id   = atoi(argv[i + 1]);
            snprintf(worker_args->shm_name, sizeof(worker_args->shm_name),
                     "%s", argv[i + 2]);
            worker_args->array_size  = atoi(argv[i + 3]);
            worker_args->num_workers = atoi(argv[i + 4]);
            worker_args->start       = atoi(argv[i + 5]);
            worker_args->length      = atoi(argv[i + 6]);
            i += 6;
            continue;
        }

        fprintf(stderr, "concur-bench: unknown option: %s\n", argv[i]);
        print_usage(argv[0]);
        return CB_ERR_ARGS;
    }

    return CB_OK;
}

cb_error_t cb_input_interactive(cb_config_t *config)
{
    if (!config) {
        return CB_ERR_ARGS;
    }

    cb_error_t err;
    long val;
    int cpu_cores = cb_cpu_count();

    /* Verbose mode (only prompt if not already set via CLI). */
    if (!config->verbose) {
        bool verbose;
        err = read_yes_no("Verbose mode (detailed per-worker output) [y/n]: ",
                          &verbose);
        if (err) return err;
        config->verbose = verbose;
    }

    /* Array length. */
    char prompt[INPUT_BUF_SIZE];
    snprintf(prompt, sizeof(prompt),
             "Array length (%d - %d): ", CB_MIN_ARRAY_LEN, INT_MAX);
    err = read_long(prompt, CB_MIN_ARRAY_LEN, INT_MAX, &val);
    if (err) return err;
    config->array_length = (int)val;

    /* Number of processes. */
    snprintf(prompt, sizeof(prompt),
             "Number of processes (%d - %d) [detected %d cores]: ",
             CB_MIN_WORKERS, CB_MAX_WORKERS, cpu_cores);
    err = read_long(prompt, CB_MIN_WORKERS, CB_MAX_WORKERS, &val);
    if (err) return err;
    config->num_processes = (int)val;

    /* Number of threads. */
    snprintf(prompt, sizeof(prompt),
             "Number of threads (%d - %d) [detected %d cores]: ",
             CB_MIN_WORKERS, CB_MAX_WORKERS, cpu_cores);
    err = read_long(prompt, CB_MIN_WORKERS, CB_MAX_WORKERS, &val);
    if (err) return err;
    config->num_threads = (int)val;

    /* Seed (unsigned -- needs read_ulong because UINT_MAX > LONG_MAX on Windows). */
    unsigned long seed_val;
    snprintf(prompt, sizeof(prompt),
             "Random seed (0 for auto, or 1 - %u): ", UINT_MAX);
    err = read_ulong(prompt, 0, UINT_MAX, &seed_val);
    if (err) return err;
    config->seed = (unsigned int)seed_val;

    /* Iterations (only prompt if using default from CLI). */
    snprintf(prompt, sizeof(prompt),
             "Benchmark iterations (1 - 100) [default %d]: ",
             config->iterations);
    err = read_long(prompt, 1, 100, &val);
    if (err) return err;
    config->iterations = (int)val;

    return CB_OK;
}
