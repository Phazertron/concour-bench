/**
 * @file platform_win.c
 * @brief Windows implementation of the platform abstraction layer.
 *
 * Provides implementations for all functions declared in platform.h
 * using Win32 APIs: CreateThread for threading, CRITICAL_SECTION for
 * mutexes, CreateProcess for process spawning, CreatePipe for IPC,
 * QueryPerformanceCounter for high-resolution timing, CreateFileMapping
 * for shared memory, and GetSystemInfo for system queries.
 *
 * This file is only compiled on Windows targets.
 */

#ifndef _WIN32
/* This file should only be compiled on Windows platforms. */
#error "platform_win.c must not be compiled on non-Windows platforms"
#endif

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ---- Compile-Time Size Assertions ---- */

_Static_assert(sizeof(CRITICAL_SECTION) <= sizeof(((cb_mutex_t *)0)->_opaque),
               "cb_mutex_t opaque buffer too small for CRITICAL_SECTION");

_Static_assert(sizeof(HANDLE) <= sizeof(((cb_thread_t *)0)->_opaque),
               "cb_thread_t opaque buffer too small for HANDLE");

_Static_assert(2 * sizeof(HANDLE) <= sizeof(((cb_pipe_t *)0)->_opaque),
               "cb_pipe_t opaque buffer too small for HANDLE[2]");

_Static_assert(sizeof(PROCESS_INFORMATION) <= sizeof(((cb_process_t *)0)->_opaque),
               "cb_process_t opaque buffer too small for PROCESS_INFORMATION");

/* ---- Internal Accessor Macros ---- */

#define CS_PTR(m)     ((CRITICAL_SECTION *)((m)->_opaque))
#define THANDLE(t)    (*((HANDLE *)((t)->_opaque)))
#define PIPE_HANDLES(p) ((HANDLE *)((p)->_opaque))
#define PI_PTR(p)     ((PROCESS_INFORMATION *)((p)->_opaque))

/**
 * @brief Internal wrapper for Windows thread entry.
 *
 * Windows threads use DWORD WINAPI (*)(LPVOID) as their entry signature,
 * while our API uses void *(*)(void *). This structure and wrapper
 * function bridge the two calling conventions.
 */
typedef struct {
    cb_thread_fn_t fn;  /**< User-provided thread function. */
    void          *arg; /**< User-provided argument. */
} win_thread_wrapper_t;

/**
 * @brief Windows thread entry trampoline.
 *
 * Allocates a win_thread_wrapper_t on the heap before thread creation,
 * then frees it here after extracting the function and argument.
 */
static DWORD WINAPI win_thread_entry(LPVOID param)
{
    win_thread_wrapper_t wrapper = *(win_thread_wrapper_t *)param;
    free(param);
    wrapper.fn(wrapper.arg);
    return 0;
}

/* ---- Timing ---- */

double cb_time_now(void)
{
    LARGE_INTEGER freq, counter;

    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0) {
        return -1.0;
    }

    if (!QueryPerformanceCounter(&counter)) {
        return -1.0;
    }

    return (double)counter.QuadPart / (double)freq.QuadPart;
}

/* ---- Mutex ---- */

cb_error_t cb_mutex_init(cb_mutex_t *mtx)
{
    memset(mtx, 0, sizeof(*mtx));
    InitializeCriticalSection(CS_PTR(mtx));
    return CB_OK;
}

cb_error_t cb_mutex_lock(cb_mutex_t *mtx)
{
    EnterCriticalSection(CS_PTR(mtx));
    return CB_OK;
}

cb_error_t cb_mutex_unlock(cb_mutex_t *mtx)
{
    LeaveCriticalSection(CS_PTR(mtx));
    return CB_OK;
}

void cb_mutex_destroy(cb_mutex_t *mtx)
{
    DeleteCriticalSection(CS_PTR(mtx));
}

/* ---- Threads ---- */

cb_error_t cb_thread_create(cb_thread_t *thread, cb_thread_fn_t fn, void *arg)
{
    memset(thread, 0, sizeof(*thread));

    win_thread_wrapper_t *wrapper = malloc(sizeof(win_thread_wrapper_t));
    if (!wrapper) {
        return CB_ERR_ALLOC;
    }

    wrapper->fn = fn;
    wrapper->arg = arg;

    HANDLE h = CreateThread(NULL, 0, win_thread_entry, wrapper, 0, NULL);
    if (h == NULL) {
        free(wrapper);
        return CB_ERR_THREAD;
    }

    THANDLE(thread) = h;
    return CB_OK;
}

cb_error_t cb_thread_join(cb_thread_t *thread)
{
    HANDLE h = THANDLE(thread);

    if (WaitForSingleObject(h, INFINITE) == WAIT_FAILED) {
        return CB_ERR_THREAD;
    }

    CloseHandle(h);
    THANDLE(thread) = NULL;
    return CB_OK;
}

/* ---- Pipes ---- */

cb_error_t cb_pipe_create(cb_pipe_t *p)
{
    memset(p, 0, sizeof(*p));

    HANDLE *handles = PIPE_HANDLES(p);
    SECURITY_ATTRIBUTES sa = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .bInheritHandle = TRUE,
        .lpSecurityDescriptor = NULL
    };

    if (!CreatePipe(&handles[0], &handles[1], &sa, 0)) {
        return CB_ERR_PIPE;
    }

    return CB_OK;
}

cb_error_t cb_pipe_write(cb_pipe_t *p, const void *buf, size_t size)
{
    HANDLE write_handle = PIPE_HANDLES(p)[1];
    const uint8_t *ptr = (const uint8_t *)buf;
    size_t remaining = size;

    while (remaining > 0) {
        DWORD written = 0;
        DWORD to_write = (remaining > (DWORD)-1) ? (DWORD)-1 : (DWORD)remaining;

        if (!WriteFile(write_handle, ptr, to_write, &written, NULL)) {
            return CB_ERR_PIPE;
        }

        ptr += written;
        remaining -= written;
    }

    return CB_OK;
}

cb_error_t cb_pipe_read(cb_pipe_t *p, void *buf, size_t size)
{
    HANDLE read_handle = PIPE_HANDLES(p)[0];
    uint8_t *ptr = (uint8_t *)buf;
    size_t remaining = size;

    while (remaining > 0) {
        DWORD bytes_read = 0;
        DWORD to_read = (remaining > (DWORD)-1) ? (DWORD)-1 : (DWORD)remaining;

        if (!ReadFile(read_handle, ptr, to_read, &bytes_read, NULL)) {
            return CB_ERR_PIPE;
        }

        if (bytes_read == 0) {
            return CB_ERR_PIPE; /* Unexpected EOF. */
        }

        ptr += bytes_read;
        remaining -= bytes_read;
    }

    return CB_OK;
}

cb_error_t cb_pipe_close_read(cb_pipe_t *p)
{
    HANDLE *handles = PIPE_HANDLES(p);

    if (handles[0] != NULL && handles[0] != INVALID_HANDLE_VALUE) {
        CloseHandle(handles[0]);
        handles[0] = NULL;
    }

    return CB_OK;
}

cb_error_t cb_pipe_close_write(cb_pipe_t *p)
{
    HANDLE *handles = PIPE_HANDLES(p);

    if (handles[1] != NULL && handles[1] != INVALID_HANDLE_VALUE) {
        CloseHandle(handles[1]);
        handles[1] = NULL;
    }

    return CB_OK;
}

/* ---- Process Spawning ---- */

cb_error_t cb_process_spawn(cb_process_t *proc,
                            const char *const *child_argv,
                            cb_child_fn_t child_fn,
                            void *child_arg)
{
    (void)child_fn;
    (void)child_arg;

    memset(proc, 0, sizeof(*proc));

    if (!child_argv || !child_argv[0]) {
        return CB_ERR_ARGS;
    }

    /* Build a single command-line string from the argv array. */
    char cmdline[4096];
    size_t offset = 0;

    for (int i = 0; child_argv[i] != NULL; i++) {
        int written;
        if (i > 0) {
            written = snprintf(cmdline + offset, sizeof(cmdline) - offset,
                               " %s", child_argv[i]);
        } else {
            written = snprintf(cmdline + offset, sizeof(cmdline) - offset,
                               "%s", child_argv[i]);
        }

        if (written < 0 || (size_t)written >= sizeof(cmdline) - offset) {
            return CB_ERR_OVERFLOW;
        }

        offset += (size_t)written;
    }

    STARTUPINFO si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION *pi = PI_PTR(proc);

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0,
                        NULL, NULL, &si, pi)) {
        return CB_ERR_FORK;
    }

    /* Close the thread handle immediately; we only need the process handle. */
    CloseHandle(pi->hThread);
    pi->hThread = NULL;

    return CB_OK;
}

cb_error_t cb_process_wait(cb_process_t *proc, int *status)
{
    PROCESS_INFORMATION *pi = PI_PTR(proc);

    if (WaitForSingleObject(pi->hProcess, INFINITE) == WAIT_FAILED) {
        return CB_ERR_PLATFORM;
    }

    if (status) {
        DWORD exit_code;
        if (GetExitCodeProcess(pi->hProcess, &exit_code)) {
            *status = (int)exit_code;
        } else {
            *status = -1;
        }
    }

    CloseHandle(pi->hProcess);
    pi->hProcess = NULL;

    return CB_OK;
}

cb_error_t cb_process_kill(cb_process_t *proc)
{
    PROCESS_INFORMATION *pi = PI_PTR(proc);

    if (pi->hProcess && !TerminateProcess(pi->hProcess, 1)) {
        return CB_ERR_PLATFORM;
    }

    return CB_OK;
}

uint32_t cb_process_get_id(const cb_process_t *proc)
{
    const PROCESS_INFORMATION *pi =
        (const PROCESS_INFORMATION *)proc->_opaque;
    return pi->dwProcessId;
}

/* ---- Shared Memory ---- */

/**
 * @brief Internal layout of the shared memory opaque buffer on Windows.
 *
 * Stores the file mapping handle and the size of the mapped region
 * alongside the base_addr pointer in cb_shared_mem_t.
 */
typedef struct {
    HANDLE mapping;  /**< Handle from CreateFileMapping. */
    size_t size;     /**< Size of the mapped region. */
} win_shm_data_t;

_Static_assert(sizeof(win_shm_data_t) <= sizeof(((cb_shared_mem_t *)0)->_opaque),
               "cb_shared_mem_t opaque buffer too small for win_shm_data_t");

#define SHM_DATA(s) ((win_shm_data_t *)((s)->_opaque))

cb_error_t cb_shared_mem_create(cb_shared_mem_t *shm,
                                const char *name,
                                size_t size)
{
    memset(shm, 0, sizeof(*shm));

    DWORD size_high = (DWORD)((uint64_t)size >> 32);
    DWORD size_low  = (DWORD)(size & 0xFFFFFFFF);

    HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                        PAGE_READWRITE,
                                        size_high, size_low, name);
    if (!mapping) {
        return CB_ERR_SHM;
    }

    void *ptr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!ptr) {
        CloseHandle(mapping);
        return CB_ERR_SHM;
    }

    shm->base_addr = ptr;
    SHM_DATA(shm)->mapping = mapping;
    SHM_DATA(shm)->size = size;

    return CB_OK;
}

cb_error_t cb_shared_mem_open(cb_shared_mem_t *shm,
                              const char *name,
                              size_t size)
{
    memset(shm, 0, sizeof(*shm));

    HANDLE mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping) {
        return CB_ERR_SHM;
    }

    void *ptr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!ptr) {
        CloseHandle(mapping);
        return CB_ERR_SHM;
    }

    shm->base_addr = ptr;
    SHM_DATA(shm)->mapping = mapping;
    SHM_DATA(shm)->size = size;

    return CB_OK;
}

void *cb_shared_mem_ptr(cb_shared_mem_t *shm)
{
    return shm->base_addr;
}

void cb_shared_mem_destroy(cb_shared_mem_t *shm)
{
    if (shm->base_addr) {
        UnmapViewOfFile(shm->base_addr);
        shm->base_addr = NULL;
    }

    win_shm_data_t *data = SHM_DATA(shm);
    if (data->mapping) {
        CloseHandle(data->mapping);
        data->mapping = NULL;
    }
}

/* ---- System Information ---- */

int cb_cpu_count(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (si.dwNumberOfProcessors > 0) ? (int)si.dwNumberOfProcessors : 1;
}

cb_error_t cb_system_info_str(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return CB_ERR_ARGS;
    }

    int cores = cb_cpu_count();

    snprintf(buf, buf_size, "Windows, %d logical cores", cores);

    return CB_OK;
}

cb_error_t cb_get_exe_path(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return CB_ERR_ARGS;
    }

    DWORD len = GetModuleFileNameA(NULL, buf, (DWORD)buf_size);
    if (len == 0 || len >= (DWORD)buf_size) {
        return CB_ERR_PLATFORM;
    }

    return CB_OK;
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

    /* Normalize forward slashes to backslashes for Windows. */
    for (size_t i = 0; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\\';
        }
    }

    /* Iterate through path components, creating each one. */
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '\\') {
            tmp[i] = '\0';

            /* Skip drive letter components like "C:" */
            if (i >= 2 && tmp[i - 1] != ':') {
                if (!CreateDirectoryA(tmp, NULL)) {
                    DWORD err = GetLastError();
                    if (err != ERROR_ALREADY_EXISTS) {
                        return CB_ERR_IO;
                    }
                }
            }

            tmp[i] = '\\';
        }
    }

    if (!CreateDirectoryA(tmp, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            return CB_ERR_IO;
        }
    }

    return CB_OK;
}
