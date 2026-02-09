/**
 * @file output.c
 * @brief Implementation of result formatting and file output.
 *
 * Generates three output formats:
 * 1. Terminal: A formatted ASCII table printed to stdout.
 * 2. Text report: A detailed human-readable report file.
 * 3. CSV: A machine-readable file for analysis tools.
 *
 * All output functions operate on the read-only cb_session_t struct
 * and have no coupling to benchmark internals.
 */

#include "output.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "platform.h"

/** @brief Separator line for the results table. */
#define TABLE_SEP \
    "+-----------+---------+------------+------------+------------+------------+---------+"

/** @brief Header line for the results table. */
#define TABLE_HDR \
    "| Mode      | Workers | Min (s)    | Mean (s)   | Max (s)    | Stddev (s) | Speedup |"

/**
 * @brief Print a single row of the results table.
 *
 * @param f       File stream (stdout or a file).
 * @param report  Benchmark report for one mode.
 * @param speedup Speedup ratio relative to the single-threaded baseline.
 */
static void print_table_row(FILE *f, const cb_run_report_t *report,
                            double speedup)
{
    fprintf(f, "| %-9s | %7d | %10.6f | %10.6f | %10.6f | %10.6f | %7.2fx |\n",
            report->label,
            report->parallelism,
            report->stats.min_sec,
            report->stats.mean_sec,
            report->stats.max_sec,
            report->stats.stddev_sec,
            speedup);
}

/**
 * @brief Print the full results table to a file stream.
 *
 * Outputs the header, separator, and one row per benchmark mode.
 *
 * @param f        File stream.
 * @param session  Complete benchmark session.
 */
static void print_table(FILE *f, const cb_session_t *session)
{
    double base_mean = session->single.stats.mean_sec;
    double speedup_proc = (base_mean > 0.0)
        ? base_mean / session->process.stats.mean_sec : 0.0;
    double speedup_thr = (base_mean > 0.0)
        ? base_mean / session->thread.stats.mean_sec : 0.0;

    fprintf(f, "%s\n", TABLE_SEP);
    fprintf(f, "%s\n", TABLE_HDR);
    fprintf(f, "%s\n", TABLE_SEP);
    print_table_row(f, &session->single, 1.0);
    print_table_row(f, &session->process, speedup_proc);
    print_table_row(f, &session->thread, speedup_thr);
    fprintf(f, "%s\n", TABLE_SEP);
}

/**
 * @brief Print the configuration summary to a file stream.
 *
 * @param f        File stream.
 * @param session  Complete benchmark session.
 */
static void print_config(FILE *f, const cb_session_t *session)
{
    const cb_config_t *c = &session->config;

    fprintf(f, "Configuration:\n");
    fprintf(f, "  Array length:    %d\n", c->array_length);
    fprintf(f, "  Processes:       %d\n", c->num_processes);
    fprintf(f, "  Threads:         %d\n", c->num_threads);
    fprintf(f, "  Seed:            %u\n", c->seed);
    fprintf(f, "  Iterations:      %d\n", c->iterations);
    fprintf(f, "  Verbose:         %s\n", c->verbose ? "yes" : "no");
}

void cb_output_terminal(const cb_session_t *session)
{
    if (!session) {
        return;
    }

    fprintf(stdout, "\n=== concur-bench Results ===\n\n");
    fprintf(stdout, "System: %s\n", session->system_info);
    fprintf(stdout, "Run:    %s\n\n", session->timestamp);

    print_config(stdout, session);
    fprintf(stdout, "\n");

    print_table(stdout, session);

    /* Correctness check. */
    if (session->single.sum == session->process.sum &&
        session->single.sum == session->thread.sum) {
        fprintf(stdout, "\nCorrectness: PASS (all modes computed sum = %ld)\n",
                session->single.sum);
    } else {
        fprintf(stdout, "\nCorrectness: FAIL\n");
        fprintf(stdout, "  single:  %ld\n", session->single.sum);
        fprintf(stdout, "  process: %ld\n", session->process.sum);
        fprintf(stdout, "  thread:  %ld\n", session->thread.sum);
    }
}

cb_error_t cb_output_create_run_dir(const char *base_dir,
                                    const char *timestamp,
                                    char *path_out,
                                    size_t path_size)
{
    if (!base_dir || !timestamp || !path_out || path_size == 0) {
        return CB_ERR_ARGS;
    }

    int written = snprintf(path_out, path_size, "%s/run_%s",
                           base_dir, timestamp);
    if (written < 0 || (size_t)written >= path_size) {
        return CB_ERR_OVERFLOW;
    }

    return cb_mkdir_p(path_out);
}

cb_error_t cb_output_txt_report(const cb_session_t *session,
                                const char *dir_path)
{
    if (!session || !dir_path) {
        return CB_ERR_ARGS;
    }

    char filepath[CB_MAX_PATH];
    int written = snprintf(filepath, sizeof(filepath), "%s/report.txt",
                           dir_path);
    if (written < 0 || (size_t)written >= sizeof(filepath)) {
        return CB_ERR_OVERFLOW;
    }

    FILE *f = fopen(filepath, "w");
    if (!f) {
        return CB_ERR_IO;
    }

    fprintf(f, "concur-bench Report\n");
    fprintf(f, "====================\n\n");
    fprintf(f, "Timestamp: %s\n", session->timestamp);
    fprintf(f, "System:    %s\n\n", session->system_info);

    print_config(f, session);
    fprintf(f, "\n");

    fprintf(f, "Results:\n\n");
    print_table(f, session);

    /* Speedup analysis. */
    double base_mean = session->single.stats.mean_sec;
    double speedup_proc = (base_mean > 0.0)
        ? base_mean / session->process.stats.mean_sec : 0.0;
    double speedup_thr = (base_mean > 0.0)
        ? base_mean / session->thread.stats.mean_sec : 0.0;

    fprintf(f, "\nSpeedup Analysis:\n");
    fprintf(f, "  Multi-process vs Single: %.2fx\n", speedup_proc);
    fprintf(f, "  Multi-thread  vs Single: %.2fx\n", speedup_thr);

    /* Correctness. */
    fprintf(f, "\nCorrectness Verification:\n");
    if (session->single.sum == session->process.sum &&
        session->single.sum == session->thread.sum) {
        fprintf(f, "  PASS - All modes computed identical sum: %ld\n",
                session->single.sum);
    } else {
        fprintf(f, "  FAIL - Sum mismatch detected:\n");
        fprintf(f, "    single:  %ld\n", session->single.sum);
        fprintf(f, "    process: %ld\n", session->process.sum);
        fprintf(f, "    thread:  %ld\n", session->thread.sum);
    }

    fclose(f);
    return CB_OK;
}

cb_error_t cb_output_csv(const cb_session_t *session,
                         const char *dir_path)
{
    if (!session || !dir_path) {
        return CB_ERR_ARGS;
    }

    char filepath[CB_MAX_PATH];
    int written = snprintf(filepath, sizeof(filepath), "%s/results.csv",
                           dir_path);
    if (written < 0 || (size_t)written >= sizeof(filepath)) {
        return CB_ERR_OVERFLOW;
    }

    FILE *f = fopen(filepath, "w");
    if (!f) {
        return CB_ERR_IO;
    }

    /* Header row. */
    fprintf(f, "mode,workers,iterations,min_sec,mean_sec,max_sec,"
               "stddev_sec,sum,speedup,array_length,seed\n");

    double base_mean = session->single.stats.mean_sec;
    const cb_run_report_t *reports[] = {
        &session->single, &session->process, &session->thread
    };

    for (int i = 0; i < 3; i++) {
        const cb_run_report_t *r = reports[i];
        double speedup = (base_mean > 0.0)
            ? base_mean / r->stats.mean_sec : 0.0;

        fprintf(f, "%s,%d,%d,%.9f,%.9f,%.9f,%.9f,%ld,%.4f,%d,%u\n",
                r->label,
                r->parallelism,
                r->stats.iterations,
                r->stats.min_sec,
                r->stats.mean_sec,
                r->stats.max_sec,
                r->stats.stddev_sec,
                r->sum,
                speedup,
                session->config.array_length,
                session->config.seed);
    }

    fclose(f);
    return CB_OK;
}

void cb_output_timestamp(char *buf, size_t buf_size)
{
    if (!buf || buf_size < 16) {
        if (buf && buf_size > 0) {
            buf[0] = '\0';
        }
        return;
    }

    time_t now = time(NULL);
    struct tm *local = localtime(&now);

    if (!local) {
        snprintf(buf, buf_size, "unknown");
        return;
    }

    strftime(buf, buf_size, "%Y%m%d_%H%M%S", local);
}
