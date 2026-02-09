/**
 * @file stats.c
 * @brief Implementation of statistical computation utilities.
 *
 * Computes min, max, mean, and sample standard deviation from an array
 * of double-precision elapsed time measurements. Uses a two-pass
 * algorithm: first pass for sum/min/max, second pass for variance.
 */

#include "stats.h"

#include <math.h>

cb_error_t cb_stats_compute(const double *times, int count,
                            cb_bench_stats_t *stats_out)
{
    if (!times || !stats_out || count < 1) {
        return CB_ERR_ARGS;
    }

    double min_val = times[0];
    double max_val = times[0];
    double sum     = 0.0;

    /* First pass: compute sum, min, and max. */
    for (int i = 0; i < count; i++) {
        sum += times[i];

        if (times[i] < min_val) {
            min_val = times[i];
        }
        if (times[i] > max_val) {
            max_val = times[i];
        }
    }

    double mean = sum / (double)count;

    /* Second pass: compute variance for standard deviation. */
    double variance_sum = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = times[i] - mean;
        variance_sum += diff * diff;
    }

    double stddev = 0.0;
    if (count > 1) {
        stddev = sqrt(variance_sum / (double)(count - 1));
    }

    stats_out->min_sec    = min_val;
    stats_out->max_sec    = max_val;
    stats_out->mean_sec   = mean;
    stats_out->stddev_sec = stddev;
    stats_out->iterations = count;

    return CB_OK;
}
