/**
 * @file    threads.h
 * @author  ln
 * @brief   c11 threads
 */

#ifndef __THREADS_H__
#define __THREADS_H__

#if defined(_WIN32)
#include <process.h>    // MSVCRT
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <limits.h>
#include <errno.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    mtx_plain       = 0,
    mtx_try         = 1,
    mtx_timed       = 2,
    mtx_recursive   = 4
};

enum {
    thrd_success = 0,   // succeeded
    thrd_timeout,       // timeout
    thrd_error,         // failed
    thrd_busy,          // resource busy
    thrd_nomem          // out of memory
};

typedef pthread_t thrd_t;
typedef pthread_key_t tss_t;

typedef int (*thrd_start_t)(void*);
typedef void (*tss_dtor_t)(void*);

extern int thrd_create(thrd_t *, thrd_start_t, void *);

extern int      mux_init(mux_t *mux);
extern mux_t*   mux_new(mux_t **mux);
extern void     mux_destroy(mux_t *mux);

extern int      mux_lock(mux_t *mux);
extern int      mux_trylock(mux_t *mux);
extern int      mux_unlock(mux_t *mux);

#ifdef __cplusplus
}
#endif

#endif // __THR_MUX__
