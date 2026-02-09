/**
 * @file platform_unix.c
 * @brief Unix/POSIX implementation of the platform abstraction layer.
 *
 * Provides implementations for all functions declared in platform.h
 * using POSIX APIs: pthreads for threading, fork/waitpid for process
 * management, pipe/read/write for inter-process communication,
 * clock_gettime(CLOCK_MONOTONIC) for high-resolution timing, and
 * sysconf for system queries.
 *
 * This file is only compiled on Unix/Linux/macOS targets.
 */

#ifdef _WIN32
/* This file should only be compiled on Unix platforms. */
#error "platform_unix.c must not be compiled on Windows"
#endif

#include "platform.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* ---- Compile-Time Size Assertions ---- */

_Static_assert(sizeof(pthread_mutex_t) <= sizeof(((cb_mutex_t *)0)->_opaque),
               "cb_mutex_t opaque buffer too small for pthread_mutex_t");

_Static_assert(sizeof(pthread_t) <= sizeof(((cb_thread_t *)0)->_opaque),
               "cb_thread_t opaque buffer too small for pthread_t");

_Static_assert(2 * sizeof(int) <= sizeof(((cb_pipe_t *)0)->_opaque),
               "cb_pipe_t opaque buffer too small for int fd[2]");

_Static_assert(sizeof(pid_t) <= sizeof(((cb_process_t *)0)->_opaque),
               "cb_process_t opaque buffer too small for pid_t");

/* ---- Internal Accessor Macros ---- */

#define MTX_PTR(m)    ((pthread_mutex_t *)((m)->_opaque))
#define THREAD_PTR(t) ((pthread_t *)((t)->_opaque))
#define PIPE_FDS(p)   ((int *)((p)->_opaque))
#define PID_PTR(p)    ((pid_t *)((p)->_opaque))

/* ---- Timing ---- */

double cb_time_now(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        return -1.0;
    }

    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* ---- Mutex ---- */

cb_error_t cb_mutex_init(cb_mutex_t *mtx)
{
    memset(mtx, 0, sizeof(*mtx));

    if (pthread_mutex_init(MTX_PTR(mtx), NULL) != 0) {
        return CB_ERR_MUTEX;
    }

    return CB_OK;
}

cb_error_t cb_mutex_lock(cb_mutex_t *mtx)
{
    if (pthread_mutex_lock(MTX_PTR(mtx)) != 0) {
        return CB_ERR_MUTEX;
    }

    return CB_OK;
}

cb_error_t cb_mutex_unlock(cb_mutex_t *mtx)
{
    if (pthread_mutex_unlock(MTX_PTR(mtx)) != 0) {
        return CB_ERR_MUTEX;
    }

    return CB_OK;
}

void cb_mutex_destroy(cb_mutex_t *mtx)
{
    pthread_mutex_destroy(MTX_PTR(mtx));
}

/* ---- Threads ---- */

cb_error_t cb_thread_create(cb_thread_t *thread, cb_thread_fn_t fn, void *arg)
{
    memset(thread, 0, sizeof(*thread));

    if (pthread_create(THREAD_PTR(thread), NULL, fn, arg) != 0) {
        return CB_ERR_THREAD;
    }

    return CB_OK;
}

cb_error_t cb_thread_join(cb_thread_t *thread)
{
    if (pthread_join(*THREAD_PTR(thread), NULL) != 0) {
        return CB_ERR_THREAD;
    }

    return CB_OK;
}

/* ---- Pipes ---- */

cb_error_t cb_pipe_create(cb_pipe_t *p)
{
    memset(p, 0, sizeof(*p));

    if (pipe(PIPE_FDS(p)) == -1) {
        return CB_ERR_PIPE;
    }

    return CB_OK;
}

cb_error_t cb_pipe_write(cb_pipe_t *p, const void *buf, size_t size)
{
    const uint8_t *ptr = (const uint8_t *)buf;
    size_t remaining = size;

    while (remaining > 0) {
        ssize_t written = write(PIPE_FDS(p)[1], ptr, remaining);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return CB_ERR_PIPE;
        }
        ptr += written;
        remaining -= (size_t)written;
    }

    return CB_OK;
}

cb_error_t cb_pipe_read(cb_pipe_t *p, void *buf, size_t size)
{
    uint8_t *ptr = (uint8_t *)buf;
    size_t remaining = size;

    while (remaining > 0) {
        ssize_t n = read(PIPE_FDS(p)[0], ptr, remaining);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return CB_ERR_PIPE;
        }
        if (n == 0) {
            return CB_ERR_PIPE; /* Unexpected EOF. */
        }
        ptr += n;
        remaining -= (size_t)n;
    }

    return CB_OK;
}

cb_error_t cb_pipe_close_read(cb_pipe_t *p)
{
    int *fds = PIPE_FDS(p);

    if (fds[0] >= 0) {
        if (close(fds[0]) == -1) {
            return CB_ERR_PIPE;
        }
        fds[0] = -1;
    }

    return CB_OK;
}

cb_error_t cb_pipe_close_write(cb_pipe_t *p)
{
    int *fds = PIPE_FDS(p);

    if (fds[1] >= 0) {
        if (close(fds[1]) == -1) {
            return CB_ERR_PIPE;
        }
        fds[1] = -1;
    }

    return CB_OK;
}

/* ---- Process Spawning ---- */

cb_error_t cb_process_spawn(cb_process_t *proc,
                            const char *const *child_argv,
                            cb_child_fn_t child_fn,
                            void *child_arg)
{
    (void)child_argv; /* Unused on Unix. */

    memset(proc, 0, sizeof(*proc));

    pid_t pid = fork();

    if (pid == -1) {
        return CB_ERR_FORK;
    }

    if (pid == 0) {
        /* Child process: execute the provided function and exit. */
        child_fn(child_arg);
        _exit(EXIT_FAILURE); /* Should not reach here; child_fn must call _exit. */
    }

    /* Parent: store the child PID. */
    *PID_PTR(proc) = pid;
    return CB_OK;
}

cb_error_t cb_process_wait(cb_process_t *proc, int *status)
{
    pid_t pid = *PID_PTR(proc);
    int wstatus;

    if (waitpid(pid, &wstatus, 0) == -1) {
        return CB_ERR_PLATFORM;
    }

    if (status) {
        if (WIFEXITED(wstatus)) {
            *status = WEXITSTATUS(wstatus);
        } else {
            *status = -1;
        }
    }

    return CB_OK;
}

cb_error_t cb_process_kill(cb_process_t *proc)
{
    pid_t pid = *PID_PTR(proc);

    if (pid > 0 && kill(pid, SIGKILL) == -1) {
        return CB_ERR_PLATFORM;
    }

    return CB_OK;
}

uint32_t cb_process_get_id(const cb_process_t *proc)
{
    return (uint32_t)(*((const pid_t *)proc->_opaque));
}

/* ---- Shared Memory (stub on Unix) ---- */

/*
 * On Unix, the process benchmark uses fork() which provides implicit
 * memory sharing via copy-on-write. These functions are provided for
 * API completeness but are not used in the Unix process benchmark.
 */

cb_error_t cb_shared_mem_create(cb_shared_mem_t *shm,
                                const char *name,
                                size_t size)
{
    (void)shm;
    (void)name;
    (void)size;
    return CB_ERR_PLATFORM; /* Not implemented on Unix. */
}

cb_error_t cb_shared_mem_open(cb_shared_mem_t *shm,
                              const char *name,
                              size_t size)
{
    (void)shm;
    (void)name;
    (void)size;
    return CB_ERR_PLATFORM; /* Not implemented on Unix. */
}

void *cb_shared_mem_ptr(cb_shared_mem_t *shm)
{
    return shm->base_addr;
}

void cb_shared_mem_destroy(cb_shared_mem_t *shm)
{
    (void)shm;
}

/* ---- System Information ---- */

int cb_cpu_count(void)
{
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return (count > 0) ? (int)count : 1;
}

cb_error_t cb_system_info_str(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return CB_ERR_ARGS;
    }

    int cores = cb_cpu_count();

#if defined(__linux__)
    FILE *f = fopen("/etc/os-release", "r");
    char distro[128] = "Linux";

    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                /* Strip PRETTY_NAME="..." */
                char *start = strchr(line, '"');
                if (start) {
                    start++;
                    char *end = strchr(start, '"');
                    if (end) {
                        *end = '\0';
                    }
                    snprintf(distro, sizeof(distro), "%s", start);
                }
                break;
            }
        }
        fclose(f);
    }

    snprintf(buf, buf_size, "%s, %d logical cores", distro, cores);
#elif defined(__APPLE__)
    snprintf(buf, buf_size, "macOS, %d logical cores", cores);
#else
    snprintf(buf, buf_size, "Unix, %d logical cores", cores);
#endif

    return CB_OK;
}

cb_error_t cb_get_exe_path(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return CB_ERR_ARGS;
    }

#if defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", buf, buf_size - 1);
    if (len == -1) {
        return CB_ERR_PLATFORM;
    }
    buf[len] = '\0';
    return CB_OK;
#elif defined(__APPLE__)
    /* On macOS, _NSGetExecutablePath is available but requires
       <mach-o/dyld.h>. For simplicity, use /proc/self/exe equivalent. */
    uint32_t size = (uint32_t)buf_size;
    extern int _NSGetExecutablePath(char *buf, uint32_t *bufsize);
    if (_NSGetExecutablePath(buf, &size) != 0) {
        return CB_ERR_PLATFORM;
    }
    return CB_OK;
#else
    (void)buf;
    (void)buf_size;
    return CB_ERR_PLATFORM;
#endif
}

cb_error_t cb_mkdir_p(const char *path)
{
    if (!path) {
        return CB_ERR_ARGS;
    }

    char tmp[CB_MAX_PATH];
    size_t len = strlen(path);

    if (len == 0 || len >= sizeof(tmp)) {
        return CB_ERR_ARGS;
    }

    memcpy(tmp, path, len + 1);

    /* Iterate through path components, creating each one. */
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (mkdir(tmp, 0755) == -1 && errno != EEXIST) {
                return CB_ERR_IO;
            }
            tmp[i] = '/';
        }
    }

    if (mkdir(tmp, 0755) == -1 && errno != EEXIST) {
        return CB_ERR_IO;
    }

    return CB_OK;
}
