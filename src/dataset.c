/**
 * @file dataset.c
 * @brief Implementation of dataset generation and management.
 *
 * Allocates a heap-resident integer array, seeds the PRNG, and fills
 * the array with pseudo-random values. Supports automatic seed
 * generation from the current time when the configured seed is 0.
 */

#include "dataset.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

cb_error_t cb_dataset_create(cb_config_t *config, int **data_out,
                             bool verbose)
{
    if (!config || !data_out) {
        return CB_ERR_ARGS;
    }

    *data_out = NULL;

    /* Generate a seed from current time if the user provided 0. */
    if (config->seed == 0) {
        srand((unsigned int)time(NULL));
        config->seed = (unsigned int)rand();
        if (verbose) {
            fprintf(stdout, "Generated random seed: %u\n", config->seed);
        }
    }

    srand(config->seed);

    /* Allocate the dataset array. */
    int *data = malloc((size_t)config->array_length * sizeof(int));
    if (!data) {
        return CB_ERR_ALLOC;
    }

    /* Populate with random values in [1, 100]. */
    if (verbose) {
        fprintf(stdout, "Populating array (%d elements): ", config->array_length);
        fflush(stdout);
    }

    int progress_step = config->array_length / 10;
    if (progress_step == 0) {
        progress_step = 1;
    }

    for (int i = 0; i < config->array_length; i++) {
        data[i] = rand() % 100 + 1;

        if (verbose && i % progress_step == 0) {
            fprintf(stdout, "+");
            fflush(stdout);
        }
    }

    if (verbose) {
        fprintf(stdout, "\n");
    }

    *data_out = data;
    return CB_OK;
}

void cb_dataset_destroy(int *data)
{
    free(data);
}
