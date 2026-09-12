// SPDX-FileCopyrightText: 2026 Ben Jarvis
// SPDX-License-Identifier: LGPL-2.1-only
#pragma once
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
typedef SRWLOCK prometheus_mutex;
#define prometheus_mutex_init(lock)    InitializeSRWLock(lock)
#define prometheus_mutex_destroy(lock) ((void) (lock))
#define prometheus_mutex_lock(lock)    AcquireSRWLockExclusive(lock)
#define prometheus_mutex_unlock(lock)  ReleaseSRWLockExclusive(lock)
typedef INIT_ONCE prometheus_once;
typedef void (*prometheus_once_fn)(
    void);
#define PROMETHEUS_ONCE_INIT INIT_ONCE_STATIC_INIT
static BOOL CALLBACK
prometheus_once_callback(
    PINIT_ONCE once,
    PVOID      param,
    PVOID     *context)
{
    (void) once;
    (void) context;
    prometheus_once_fn *fn = param;
    (*fn)();
    return TRUE;
} // prometheus_once_callback
static inline void
prometheus_call_once(
    prometheus_once   *once,
    prometheus_once_fn fn)
{
    InitOnceExecuteOnce(once, prometheus_once_callback, &fn, NULL);
} // prometheus_call_once
#define PROMETHEUS_ALIGN __declspec(align(64))
#else // ifdef _WIN32
#include <pthread.h>
typedef pthread_mutex_t prometheus_mutex;
#define prometheus_mutex_init(lock)    pthread_mutex_init(lock, NULL)
#define prometheus_mutex_destroy(lock) pthread_mutex_destroy(lock)
#define prometheus_mutex_lock(lock)    pthread_mutex_lock(lock)
#define prometheus_mutex_unlock(lock)  pthread_mutex_unlock(lock)
typedef pthread_once_t prometheus_once;
#define PROMETHEUS_ONCE_INIT PTHREAD_ONCE_INIT
#define prometheus_call_once(once, fn) pthread_once(once, fn)
#define PROMETHEUS_ALIGN     __attribute__((aligned(64)))
#endif // ifdef _WIN32

static inline void *
prometheus_handle_alloc(size_t size)
{
    void *ptr;

#ifdef _WIN32
    ptr = _aligned_malloc(size, 64);
#else // ifdef _WIN32
    if (posix_memalign(&ptr, 64, size) != 0) {
        ptr = NULL;
    }
#endif // ifdef _WIN32
    if (!ptr) {
        abort();
    }
    memset(ptr, 0, size);
    return ptr;
} // prometheus_handle_alloc
static inline void
prometheus_handle_free(void *ptr)
{
#ifdef _WIN32
    _aligned_free(ptr);
#else // ifdef _WIN32
    free(ptr);
#endif // ifdef _WIN32
} // prometheus_handle_free
