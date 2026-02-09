/**
 * @file platform.h
 * @brief Cross-platform abstraction layer for concur-bench.
 *
 * Provides unified types and function signatures for operations that
 * differ between Unix and Windows: high-resolution timing, threads,
 * mutexes, pipes, process spawning, shared memory, and system queries.
 *
 * Implementations reside in platform_unix.c and platform_win.c; only
 * one is compiled per target via CMake. All platform-specific headers
 * are included exclusively in the .c files, never here.
 *
 * Types use fixed-size opaque byte arrays sized to hold the larger of
 * the Unix and Windows representations. Each platform .c file includes
 * a _Static_assert to verify the opaque buffer is large enough.
 */

#ifndef CB_PLATFORM_H
#define CB_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#include "error.h"

/* ---- Platform Detection ---- */

#ifdef _WIN32
    #define CB_PLATFORM_WINDOWS
#else
    #define CB_PLATFORM_UNIX
#endif

/** @brief Maximum path length for output directories and file names. */
#define CB_MAX_PATH  512

/* ---- Opaque Platform Types ---- */

/**
 * @brief Opaque mutex type.
 *
 * Unix: wraps pthread_mutex_t (typically 40 bytes on 64-bit Linux).
 * Windows: wraps CRITICAL_SECTION (typically 40 bytes on 64-bit Windows).
 * The 64-byte buffer provides padding for alignment and future growth.
 */
typedef struct {
    uint8_t _opaque[64];
} cb_mutex_t;

/**
 * @brief Opaque thread handle.
 *
 * Unix: wraps pthread_t (8 bytes on 64-bit Linux).
 * Windows: wraps HANDLE (8 bytes on 64-bit Windows).
 */
typedef struct {
    uint8_t _opaque[16];
} cb_thread_t;

/**
 * @brief Opaque pipe type (bidirectional: has a read end and a write end).
 *
 * Unix: wraps int fd[2] (8 bytes).
 * Windows: wraps two HANDLEs (16 bytes).
 */
typedef struct {
    uint8_t _opaque[16];
} cb_pipe_t;

/**
 * @brief Opaque process handle.
 *
 * Unix: wraps pid_t (4 bytes).
 * Windows: wraps PROCESS_INFORMATION (24 bytes on 64-bit).
 */
typedef struct {
    uint8_t _opaque[32];
} cb_process_t;

/**
 * @brief Opaque shared memory region.
 *
 * Used on Windows for the process benchmark (CreateFileMapping +
 * MapViewOfFile). On Unix, fork() provides implicit memory sharing
 * via copy-on-write, so this type is typically unused.
 *
 * @note base_addr is exposed for direct pointer access after mapping.
 */
typedef struct {
    void    *base_addr;   /**< Pointer to the mapped memory region. */
    uint8_t  _opaque[40]; /**< Platform-specific handles and metadata. */
} cb_shared_mem_t;

/** @brief Function signature for thread entry points. */
typedef void *(*cb_thread_fn_t)(void *arg);

/**
 * @brief Function signature for Unix child process entry points.
 *
 * On Unix, cb_process_spawn() calls fork() and invokes this function
 * in the child process. The function must not return; it must call
 * _exit() when done. On Windows, this parameter is ignored.
 */
typedef void (*cb_child_fn_t)(void *arg);

/* ---- Timing ---- */

/**
 * @brief Return a monotonic timestamp in seconds.
 *
 * Uses CLOCK_MONOTONIC on Unix and QueryPerformanceCounter on Windows.
 * Monotonic clocks are immune to NTP adjustments and system clock
 * changes, making them suitable for benchmarking.
 *
 * @return Seconds since an arbitrary epoch, or -1.0 on error.
 */
double cb_time_now(void);

/* ---- Mutex ---- */

/**
 * @brief Initialize a mutex.
 * @param mtx  Pointer to the mutex to initialize. Must not be NULL.
 * @return CB_OK on success, CB_ERR_MUTEX on failure.
 */
cb_error_t cb_mutex_init(cb_mutex_t *mtx);

/**
 * @brief Acquire (lock) a mutex. Blocks until the mutex is available.
 * @param mtx  Pointer to an initialized mutex.
 * @return CB_OK on success, CB_ERR_MUTEX on failure.
 */
cb_error_t cb_mutex_lock(cb_mutex_t *mtx);

/**
 * @brief Release (unlock) a mutex.
 * @param mtx  Pointer to a locked mutex.
 * @return CB_OK on success, CB_ERR_MUTEX on failure.
 */
cb_error_t cb_mutex_unlock(cb_mutex_t *mtx);

/**
 * @brief Destroy a mutex and release associated resources.
 * @param mtx  Pointer to an initialized mutex. Safe to call on a
 *             zero-initialized (never-init'd) mutex.
 */
void cb_mutex_destroy(cb_mutex_t *mtx);

/* ---- Threads ---- */

/**
 * @brief Create and start a new thread.
 *
 * The thread begins executing fn(arg) immediately upon creation.
 *
 * @param thread  Output handle, filled on success.
 * @param fn      Thread entry function.
 * @param arg     Argument passed to fn.
 * @return CB_OK on success, CB_ERR_THREAD on failure.
 */
cb_error_t cb_thread_create(cb_thread_t *thread, cb_thread_fn_t fn, void *arg);

/**
 * @brief Wait for a thread to finish executing.
 *
 * Blocks the calling thread until the specified thread terminates.
 *
 * @param thread  Handle from a previous cb_thread_create() call.
 * @return CB_OK on success, CB_ERR_THREAD on failure.
 */
cb_error_t cb_thread_join(cb_thread_t *thread);

/* ---- Pipes ---- */

/**
 * @brief Create a unidirectional pipe.
 *
 * The pipe has a read end and a write end. Data written to the write
 * end can be read from the read end. Both ends must be explicitly
 * closed when no longer needed.
 *
 * @param p  Output pipe handle, filled on success.
 * @return CB_OK on success, CB_ERR_PIPE on failure.
 */
cb_error_t cb_pipe_create(cb_pipe_t *p);

/**
 * @brief Write data to the write end of a pipe.
 * @param p     Pipe handle.
 * @param buf   Source buffer.
 * @param size  Number of bytes to write.
 * @return CB_OK on success, CB_ERR_PIPE on failure.
 */
cb_error_t cb_pipe_write(cb_pipe_t *p, const void *buf, size_t size);

/**
 * @brief Read data from the read end of a pipe.
 *
 * Blocks until exactly @p size bytes are available or an error occurs.
 *
 * @param p     Pipe handle.
 * @param buf   Destination buffer.
 * @param size  Number of bytes to read.
 * @return CB_OK on success, CB_ERR_PIPE on failure.
 */
cb_error_t cb_pipe_read(cb_pipe_t *p, void *buf, size_t size);

/**
 * @brief Close the read end of a pipe.
 * @param p  Pipe handle.
 * @return CB_OK on success, CB_ERR_PIPE on failure.
 */
cb_error_t cb_pipe_close_read(cb_pipe_t *p);

/**
 * @brief Close the write end of a pipe.
 * @param p  Pipe handle.
 * @return CB_OK on success, CB_ERR_PIPE on failure.
 */
cb_error_t cb_pipe_close_write(cb_pipe_t *p);

/* ---- Process Spawning ---- */

/**
 * @brief Spawn a child process.
 *
 * On Unix: calls fork(). In the child, invokes child_fn(child_arg),
 * which must call _exit() and never return. The child_argv parameter
 * is ignored on Unix.
 *
 * On Windows: calls CreateProcess() with the arguments in child_argv.
 * The child_fn and child_arg parameters are ignored on Windows. The
 * first element of child_argv must be the executable path.
 *
 * @param proc       Output process handle, filled on success.
 * @param child_argv NULL-terminated argument array (Windows only).
 * @param child_fn   Function to run in the child (Unix only).
 * @param child_arg  Argument for child_fn (Unix only).
 * @return CB_OK on success, CB_ERR_FORK on failure.
 */
cb_error_t cb_process_spawn(cb_process_t *proc,
                            const char *const *child_argv,
                            cb_child_fn_t child_fn,
                            void *child_arg);

/**
 * @brief Wait for a child process to terminate.
 *
 * Blocks until the child exits. The exit status is stored in @p status.
 *
 * @param proc    Process handle from cb_process_spawn().
 * @param status  Output exit status (0 = success). May be NULL.
 * @return CB_OK on success, CB_ERR_PLATFORM on failure.
 */
cb_error_t cb_process_wait(cb_process_t *proc, int *status);

/**
 * @brief Forcefully terminate a child process.
 * @param proc  Process handle.
 * @return CB_OK on success, CB_ERR_PLATFORM on failure.
 */
cb_error_t cb_process_kill(cb_process_t *proc);

/**
 * @brief Get the numeric process ID of a spawned process.
 * @param proc  Process handle.
 * @return The PID (Unix) or process ID (Windows).
 */
uint32_t cb_process_get_id(const cb_process_t *proc);

/* ---- Shared Memory ---- */

/**
 * @brief Create a named shared memory region.
 *
 * Allocates a shared memory segment accessible by name from other
 * processes. After creation, cb_shared_mem_ptr() returns a pointer
 * to the mapped memory.
 *
 * @param shm   Output handle, filled on success.
 * @param name  Unique name for the shared memory segment.
 * @param size  Size in bytes.
 * @return CB_OK on success, CB_ERR_SHM on failure.
 */
cb_error_t cb_shared_mem_create(cb_shared_mem_t *shm,
                                const char *name,
                                size_t size);

/**
 * @brief Open an existing named shared memory region.
 *
 * Used by child/worker processes to access memory created by the parent.
 *
 * @param shm   Output handle, filled on success.
 * @param name  Name of the existing shared memory segment.
 * @param size  Expected size in bytes.
 * @return CB_OK on success, CB_ERR_SHM on failure.
 */
cb_error_t cb_shared_mem_open(cb_shared_mem_t *shm,
                              const char *name,
                              size_t size);

/**
 * @brief Get a pointer to the mapped shared memory region.
 * @param shm  Shared memory handle.
 * @return Pointer to the mapped memory, or NULL if not mapped.
 */
void *cb_shared_mem_ptr(cb_shared_mem_t *shm);

/**
 * @brief Unmap and release a shared memory region.
 *
 * After this call, the handle is invalidated and the memory pointer
 * returned by cb_shared_mem_ptr() must not be dereferenced.
 *
 * @param shm  Shared memory handle. Safe to call on a zero-initialized handle.
 */
void cb_shared_mem_destroy(cb_shared_mem_t *shm);

/* ---- System Information ---- */

/**
 * @brief Return the number of logical CPU cores available.
 *
 * Uses sysconf(_SC_NPROCESSORS_ONLN) on Unix and GetSystemInfo() on
 * Windows. Returns 1 if detection fails.
 *
 * @return Number of logical CPU cores (minimum 1).
 */
int cb_cpu_count(void);

/**
 * @brief Fill a buffer with a human-readable OS and CPU description.
 *
 * Example output: "Linux 6.1.0 x86_64, 8 cores"
 *
 * @param buf       Output buffer.
 * @param buf_size  Size of the output buffer in bytes.
 * @return CB_OK on success, CB_ERR_PLATFORM on failure.
 */
cb_error_t cb_system_info_str(char *buf, size_t buf_size);

/**
 * @brief Get the filesystem path of the currently running executable.
 *
 * Used on Windows to respawn the same executable with --worker
 * arguments for the process benchmark.
 *
 * @param buf       Output buffer for the path.
 * @param buf_size  Size of the output buffer in bytes.
 * @return CB_OK on success, CB_ERR_PLATFORM on failure.
 */
cb_error_t cb_get_exe_path(char *buf, size_t buf_size);

/**
 * @brief Create a directory (and parent directories if needed).
 *
 * Equivalent to "mkdir -p" on Unix. On Windows, creates each
 * component of the path that does not already exist.
 *
 * @param path  Directory path to create.
 * @return CB_OK on success (or if already exists), CB_ERR_IO on failure.
 */
cb_error_t cb_mkdir_p(const char *path);

#endif /* CB_PLATFORM_H */
