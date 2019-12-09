/**
 * @file    threads_c11.h
 * @author  ln
 * @brief   C11 <threads.h> emulation library for Linux & Visual Studio
 */

#ifndef __THREADS_C11_H__
#define __THREADS_C11_H__

#if defined(_WIN32) && !defined(__CYGWIN__)     // Visual Studio

#include <thr/threads.h>

#define TIME_MONO                       TIME_UTC
#define timespec_get(tm, base)          xtime_get((xtime *)(tm), base)

#undef  cnd_timedwait
#define cnd_timedwait(cnd, mtx, tm)     _Cnd_timedwait((*cnd), (*mtx), (xtime *)tm)

#else   // linux

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <stdint.h>     /* for intptr_t */

#include <pthread.h>

enum {
    mtx_plain       = 0x1,
    mtx_try         = 0x2,
    mtx_timed       = 0x4,
    mtx_recursive   = 0x100
};

enum {
    thrd_success = 0,   // succeeded
    thrd_nomem,         // out of memory
    thrd_timeout,       // timeout
    thrd_busy,          // resource busy
    thrd_error          // failed
};

typedef int (*thrd_start_t)(void*);
typedef void (*tss_dtor_t)(void*);

#define ONCE_FLAG_INIT          PTHREAD_ONCE_INIT
#define TSS_DTOR_ITERATIONS     PTHREAD_DESTRUCTOR_ITERATIONS

#define TIME_MONO               15

#define CND_TIME_DEF            TIME_MONO   // TIME_MONO or TIME_UTC
#define cnd_init(cond)          __cnd_init(cond, CND_TIME_DEF)

typedef struct {
    pthread_cond_t      cnd;
    pthread_condattr_t  cnd_attr;
} cnd_t;

typedef pthread_t       thrd_t;
typedef pthread_key_t   tss_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_once_t  once_flag;

extern void call_once(once_flag *flag, void (*func)(void));

/* thread */
extern int      thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
extern void     thrd_exit(int res);
extern int      thrd_join(thrd_t thr, int *res);
extern thrd_t   thrd_current(void);
extern int      thrd_detach(thrd_t thr);
extern int      thrd_equal(thrd_t thr0, thrd_t thr1);
extern void     thrd_sleep(const struct timespec *time_point, struct timespec *remaining);
extern void     thrd_yield(void);

/* condition variable */
extern int      cnd_broadcast(cnd_t *cond);
extern void     cnd_destroy(cnd_t *cond);
extern int      __cnd_init(cnd_t *cond, int base);
extern int      cnd_signal(cnd_t *cond);
extern int      cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *abs_time);
extern int      cnd_wait(cnd_t *cond, mtx_t *mtx);

/* mutex lock */
extern int      mtx_init(mtx_t *mtx, int type);
extern void     mtx_destroy(mtx_t *mtx);
extern int      mtx_lock(mtx_t *mtx);
extern int      mtx_trylock(mtx_t *mtx);
extern int      mtx_timedlock(mtx_t *mtx, const struct timespec *ts);
extern int      mtx_unlock(mtx_t *mtx);

/* thread-specific storage */
extern int      tss_create(tss_t *key, tss_dtor_t dtor);
extern void     tss_delete(tss_t key);
extern void*    tss_get(tss_t key);
extern int      tss_set(tss_t key, void *val);

#ifdef __cplusplus
}
#endif

#endif  // defined(_WIN32) && !defined(__CYGWIN__)

#endif  // __THREADS_C11_H__

