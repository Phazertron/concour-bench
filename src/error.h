/**
 * @file error.h
 * @brief Error handling for concur-bench.
 *
 * Defines a centralized error code enumeration and utility functions for
 * reporting errors consistently throughout the application. Every function
 * that can fail returns a cb_error_t value; CB_OK (0) indicates success,
 * and all error codes are negative integers for easy boolean checking.
 */

#ifndef CB_ERROR_H
#define CB_ERROR_H

#include <stddef.h>

/**
 * @brief Error codes returned by all concur-bench functions.
 *
 * Functions return CB_OK (0) on success. All error codes are negative
 * to allow concise boolean checking:
 * @code
 *     cb_error_t err = cb_some_function(...);
 *     if (err) { cb_perror("context", err); goto cleanup; }
 * @endcode
 */
typedef enum {
    CB_OK            =   0, /**< Operation completed successfully. */
    CB_ERR_ALLOC     =  -1, /**< Memory allocation (malloc/calloc/realloc) failed. */
    CB_ERR_PIPE      =  -2, /**< Pipe creation, read, write, or close failed. */
    CB_ERR_FORK      =  -3, /**< Process creation (fork or CreateProcess) failed. */
    CB_ERR_THREAD    =  -4, /**< Thread creation or join failed. */
    CB_ERR_MUTEX     =  -5, /**< Mutex initialization, lock, or unlock failed. */
    CB_ERR_IO        =  -6, /**< File I/O or directory creation failed. */
    CB_ERR_INPUT     =  -7, /**< Invalid user input or parse failure. */
    CB_ERR_PLATFORM  =  -8, /**< Platform-specific system call failed. */
    CB_ERR_TIMEOUT   =  -9, /**< Operation timed out. */
    CB_ERR_OVERFLOW  = -10, /**< Integer or buffer overflow detected. */
    CB_ERR_ARGS      = -11, /**< Invalid function arguments (NULL pointer, bad range). */
    CB_ERR_SHM       = -12  /**< Shared memory creation or mapping failed. */
} cb_error_t;

/**
 * @brief Convert an error code to a human-readable string.
 *
 * The returned pointer is to a static string literal and must not be freed.
 *
 * @param err  The error code to describe.
 * @return A static string describing the error. Never returns NULL.
 */
const char *cb_error_str(cb_error_t err);

/**
 * @brief Print a formatted error message to stderr.
 *
 * Output format: "concur-bench: <prefix>: <error description>\n"
 * If the system errno is set (nonzero), appends ": <strerror(errno)>".
 *
 * @param prefix  Context string identifying where the error occurred
 *                (e.g., function name or operation description).
 * @param err     The cb_error_t error code.
 */
void cb_perror(const char *prefix, cb_error_t err);

#endif /* CB_ERROR_H */
