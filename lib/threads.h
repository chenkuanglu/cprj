/**
 * @file    threads.h
 * @author  ln
 * @brief   C11 <threads.h> emulation library
 */

#ifndef __THREADS_H__
#define __THREADS_H__

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

typedef int (*thrd_start_t)(void*);
typedef void (*tss_dtor_t)(void*);

#if defined(_WIN32) && !defined(__CYGWIN__)
#include "threads_win32.h"
#else
#include "threads_posix.h"
#endif

#ifdef __cplusplus
}
#endif

#endif // __THREADS_H__

