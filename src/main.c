/**
 * @file main.c
 * @brief Entry point for concur-bench.
 *
 * Orchestrates the full benchmark pipeline:
 * 1. Parse command-line arguments (including --worker dispatch on Windows).
 * 2. Collect remaining configuration interactively from the user.
 * 3. Generate the random dataset.
 * 4. Run three benchmark modes: single-threaded, multi-process, multi-thread.
 * 5. Verify correctness (all modes must produce the same sum).
 * 6. Display results on the terminal.
 * 7. Save a text report and CSV file to the results directory.
 *
 * On Windows, if the --worker flag is detected, the process is a child
 * spawned by the multi-process benchmark. In that case, control is
 * immediately dispatched to cb_bench_process_worker_main() and the
 * normal benchmark flow is skipped.
 */

#include <stdio.h>
#include <string.h>

#include "bench_process.h"
#include "bench_single.h"
#include "bench_thread.h"
#include "dataset.h"
#include "error.h"
#include "input.h"
#include "output.h"
#include "platform.h"
#include "types.h"

int main(int argc, char *argv[])
{
    cb_error_t err = CB_OK;
    cb_config_t config;
    cb_session_t session;
    cb_worker_args_t worker_args;
    bool is_worker = false;
    int *dataset = NULL;
    char run_dir[CB_MAX_PATH];

    memset(&config, 0, sizeof(config));
    memset(&session, 0, sizeof(session));
    memset(&worker_args, 0, sizeof(worker_args));
    memset(run_dir, 0, sizeof(run_dir));

    /* ---- Step 1: Parse command-line arguments ---- */
    err = cb_parse_args(argc, argv, &config, &is_worker, &worker_args);
    if (err) {
        /* --help returns CB_ERR_ARGS after printing usage; exit cleanly. */
        return (err == CB_ERR_ARGS) ? 0 : 1;
    }

    /* ---- Step 2: Windows worker dispatch ---- */
#ifdef _WIN32
    if (is_worker) {
        return cb_bench_process_worker_main(&worker_args);
    }
#else
    if (is_worker) {
        fprintf(stderr, "concur-bench: --worker flag is only used "
                        "internally on Windows.\n");
        return 1;
    }
#endif

    /* ---- Step 3: Interactive input for remaining config ---- */
    fprintf(stdout, "concur-bench - Concurrency Benchmark Tool\n");
    fprintf(stdout, "==========================================\n\n");

    err = cb_input_interactive(&config);
    if (err) {
        cb_perror("input", err);
        return 1;
    }

    /* ---- Step 4: Generate dataset ---- */
    fprintf(stdout, "\n");
    err = cb_dataset_create(&config, &dataset, config.verbose);
    if (err) {
        cb_perror("dataset creation", err);
        return 1;
    }

    /* ---- Step 5: Populate session metadata ---- */
    session.config = config;
    cb_system_info_str(session.system_info, sizeof(session.system_info));
    cb_output_timestamp(session.timestamp, sizeof(session.timestamp));

    /* ---- Step 6: Run benchmarks ---- */
    fprintf(stdout, "\nRunning single-threaded benchmark (%d iteration%s)...\n",
            config.iterations, config.iterations == 1 ? "" : "s");
    err = cb_bench_single_run(dataset, &config, &session.single);
    if (err) {
        cb_perror("single-threaded benchmark", err);
        goto cleanup;
    }

    fprintf(stdout, "Running multi-process benchmark (%d process%s, "
            "%d iteration%s)...\n",
            config.num_processes, config.num_processes == 1 ? "" : "es",
            config.iterations, config.iterations == 1 ? "" : "s");
    err = cb_bench_process_run(dataset, &config, &session.process);
    if (err) {
        cb_perror("multi-process benchmark", err);
        goto cleanup;
    }

    fprintf(stdout, "Running multi-threaded benchmark (%d thread%s, "
            "%d iteration%s)...\n",
            config.num_threads, config.num_threads == 1 ? "" : "s",
            config.iterations, config.iterations == 1 ? "" : "s");
    err = cb_bench_thread_run(dataset, &config, &session.thread);
    if (err) {
        cb_perror("multi-threaded benchmark", err);
        goto cleanup;
    }

    /* ---- Step 7: Display results ---- */
    cb_output_terminal(&session);

    /* ---- Step 8: Save report files ---- */
    err = cb_output_create_run_dir("results", session.timestamp,
                                   run_dir, sizeof(run_dir));
    if (err) {
        cb_perror("creating results directory", err);
        /* Non-fatal: results were already shown on terminal. */
        err = CB_OK;
    } else {
        cb_error_t txt_err = cb_output_txt_report(&session, run_dir);
        cb_error_t csv_err = cb_output_csv(&session, run_dir);

        if (txt_err) {
            cb_perror("writing text report", txt_err);
        }
        if (csv_err) {
            cb_perror("writing CSV file", csv_err);
        }

        if (!txt_err && !csv_err) {
            fprintf(stdout, "\nResults saved to: %s/\n", run_dir);
        }
    }

cleanup:
    cb_dataset_destroy(dataset);
    return (err != CB_OK) ? 1 : 0;
}
