/**
 * @file error.c
 * @brief Implementation of error handling utilities for concur-bench.
 *
 * Provides human-readable error descriptions and a centralized error
 * reporting function that writes to stderr with optional system errno
 * context.
 */

#include "error.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

const char *cb_error_str(cb_error_t err)
{
    switch (err) {
    case CB_OK:           return "success";
    case CB_ERR_ALLOC:    return "memory allocation failed";
    case CB_ERR_PIPE:     return "pipe operation failed";
    case CB_ERR_FORK:     return "process creation failed";
    case CB_ERR_THREAD:   return "thread operation failed";
    case CB_ERR_MUTEX:    return "mutex operation failed";
    case CB_ERR_IO:       return "I/O operation failed";
    case CB_ERR_INPUT:    return "invalid input";
    case CB_ERR_PLATFORM: return "platform-specific error";
    case CB_ERR_TIMEOUT:  return "operation timed out";
    case CB_ERR_OVERFLOW: return "overflow detected";
    case CB_ERR_ARGS:     return "invalid arguments";
    case CB_ERR_SHM:      return "shared memory operation failed";
    }

    return "unknown error";
}

void cb_perror(const char *prefix, cb_error_t err)
{
    int saved_errno = errno;

    if (prefix && saved_errno != 0) {
        fprintf(stderr, "concur-bench: %s: %s: %s\n",
                prefix, cb_error_str(err), strerror(saved_errno));
    } else if (prefix) {
        fprintf(stderr, "concur-bench: %s: %s\n",
                prefix, cb_error_str(err));
    } else {
        fprintf(stderr, "concur-bench: %s\n", cb_error_str(err));
    }
}
