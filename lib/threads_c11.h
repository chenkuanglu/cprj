/**
 * @file    threads_c11.h
 * @author  ln
 * @brief   C11 <threads.h> emulation library for Linux & Visual Studio
 */

#ifndef __THREADS_C11_H__
#define __THREADS_C11_H__

#if defined(_WIN32) && !defined(__CYGWIN__)

#include "thr/threads.h"
#define TIME_MONO TIME_UTC
#undef  cnd_timedwait

#define cnd_init_r(cnd, base)           cnd_init(cnd)
#define cnd_timedwait(cnd, mtx, tm)     _Cnd_timedwait((*cnd), (*mtx), (xtime *)tm)
#define timespec_get(tm, base)          xtime_get((xtime *)(tm), base)
#define timespec_get_r(tm, base)        xtime_get((xtime *)(tm), base)

#else

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <stdint.h> /* for intptr_t */

/*
 * EMULATED_THREADS_USE_NATIVE_TIMEDLOCK
 * Use pthread_mutex_timedlock() for `mtx_timedlock()'
 * Otherwise use mtx_trylock() + *busy loop* emulation.
 */
#if !defined(__CYGWIN__) && !defined(__APPLE__) && !defined(__NetBSD__)
#define EMULATED_THREADS_USE_NATIVE_TIMEDLOCK
#endif

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

#define ONCE_FLAG_INIT PTHREAD_ONCE_INIT
#define TSS_DTOR_ITERATIONS PTHREAD_DESTRUCTOR_ITERATIONS

#define TIME_MONO 15

typedef struct {
    pthread_cond_t      cnd;
    pthread_condattr_t  cnd_attr;
} cnd_t;

typedef pthread_t       thrd_t;
typedef pthread_key_t   tss_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_once_t  once_flag;

struct impl_thrd_param {
    thrd_start_t func;
    void *arg;
};

static inline void* impl_thrd_routine(void *p)
{
    struct impl_thrd_param pack = *((struct impl_thrd_param *)p);
    free(p);
    return (void*)(intptr_t)pack.func(pack.arg);
}

static inline void call_once(once_flag *flag, void (*func)(void))
{
    pthread_once(flag, func);
}

/* thread */

static inline int thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
    if (thr == NULL || func == NULL) {
        errno = EINVAL;
        return thrd_error;
    }
    struct impl_thrd_param *pack;
    pack = (struct impl_thrd_param *)malloc(sizeof(struct impl_thrd_param));
    if (!pack) return thrd_nomem;
    pack->func = func;
    pack->arg = arg;
    if (pthread_create(thr, NULL, impl_thrd_routine, pack) != 0) {
        free(pack);
        return thrd_error;
    }
    return thrd_success;
}

static inline void thrd_exit(int res)
{
    pthread_exit((void*)(intptr_t)res);
}

static inline int thrd_join(thrd_t thr, int *res)
{
    void *code;
    if (pthread_join(thr, &code) != 0)
        return thrd_error;
    if (res)
        *res = (int)(intptr_t)code;
    return thrd_success;
}

static inline thrd_t thrd_current(void)
{
    return pthread_self();
}

static inline int thrd_detach(thrd_t thr)
{
    return (pthread_detach(thr) == 0) ? thrd_success : thrd_error;
}

static inline int thrd_equal(thrd_t thr0, thrd_t thr1)
{
    return pthread_equal(thr0, thr1);
}

static inline void thrd_sleep(const struct timespec *time_point, struct timespec *remaining)
{
    if (time_point != NULL)
        nanosleep(time_point, remaining);
}

static inline void thrd_yield(void)
{
    sched_yield();
}

/* condition variable */

static inline int cnd_broadcast(cnd_t *cond)
{
    return (pthread_cond_broadcast(&cond->cnd) == 0) ? thrd_success : thrd_error;
}

static inline void cnd_destroy(cnd_t *cond)
{
    pthread_cond_destroy(&cond->cnd);
    pthread_condattr_destroy(&cond->cnd_attr);
}

static inline int cnd_init(cnd_t *cond)
{
    if (cond == NULL) {
        errno = EINVAL;
        return thrd_error;
    }
    pthread_condattr_init(&cond->cnd_attr);
    return (pthread_cond_init(&cond->cnd, NULL) == 0) ? thrd_success : thrd_error;
}

static inline int cnd_init_r(cnd_t *cond, int base)
{
    if (cond == NULL) {
        errno = EINVAL;
        return thrd_error;
    }
    if (base == TIME_UTC) {
        pthread_condattr_init(&cond->cnd_attr);
        return (pthread_cond_init(&cond->cnd, NULL) == 0) ? thrd_success : thrd_error;
    } else if (base == TIME_MONO) {
        pthread_condattr_init(&cond->cnd_attr);
        if ((errno=pthread_condattr_setclock(&cond->cnd_attr, CLOCK_MONOTONIC)) != 0)
            return thrd_error;
        return (pthread_cond_init(&cond->cnd, &cond->cnd_attr) == 0) ? thrd_success : thrd_error;
    } else {
        return thrd_error;
    }
}

static inline int cnd_signal(cnd_t *cond)
{
    return (pthread_cond_signal(&cond->cnd) == 0) ? thrd_success : thrd_error;
}

static inline int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *abs_time)
{
    int rt = pthread_cond_timedwait(&cond->cnd, mtx, abs_time);
    if (rt == ETIMEDOUT)
        return thrd_busy;
    return (rt == 0) ? thrd_success : thrd_error;
}

static inline int cnd_wait(cnd_t *cond, mtx_t *mtx)
{
    return (pthread_cond_wait(&cond->cnd, mtx) == 0) ? thrd_success : thrd_error;
}


/* mutex lock */

static inline int mtx_init(mtx_t *mtx, int type)
{
    if (mtx == NULL) {
        errno = EINVAL;
        return thrd_error;
    }
    if (type != mtx_plain && type != mtx_timed && type != mtx_try
                                && type != (mtx_plain|mtx_recursive)
                                && type != (mtx_timed|mtx_recursive)
                                && type != (mtx_try|mtx_recursive)) {
        return thrd_error;
    }

    if ((type & mtx_recursive) == 0) {
        pthread_mutex_init(mtx, NULL);
        return thrd_success;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    return thrd_success;
}

static inline void mtx_destroy(mtx_t *mtx)
{
    pthread_mutex_destroy(mtx);
}

static inline int mtx_lock(mtx_t *mtx)
{
    return (pthread_mutex_lock(mtx) == 0) ? thrd_success : thrd_error;
}

static inline int mtx_trylock(mtx_t *mtx)
{
    return (pthread_mutex_trylock(mtx) == 0) ? thrd_success : thrd_busy;
}

static inline int mtx_timedlock(mtx_t *mtx, const struct timespec *ts)
{
    if (mtx == NULL || ts == NULL) {
        errno = EINVAL;
        return thrd_error;
    }

    {
#ifdef EMULATED_THREADS_USE_NATIVE_TIMEDLOCK
    int rt;
    rt = pthread_mutex_timedlock(mtx, ts);
    if (rt == 0) {
        return thrd_success;
    }
    return (rt == ETIMEDOUT) ? thrd_busy : thrd_error;
#else
    time_t expire = time(NULL);
    expire += ts->tv_sec;
    while (mtx_trylock(mtx) != thrd_success) {
        time_t now = time(NULL);
        if (expire < now) {
            return thrd_busy;
        }
        // busy loop!
        thrd_yield();
    }
    return thrd_success;
#endif
    }
}

static inline int mtx_unlock(mtx_t *mtx)
{
    return (pthread_mutex_unlock(mtx) == 0) ? thrd_success : thrd_error;
}

/* thread-specific storage */

static inline int tss_create(tss_t *key, tss_dtor_t dtor)
{
    if (key == NULL) {
        errno = EINVAL;
        return thrd_error;
    }
    return (pthread_key_create(key, dtor) == 0) ? thrd_success : thrd_error;
}

static inline void tss_delete(tss_t key)
{
    pthread_key_delete(key);
}

static inline void* tss_get(tss_t key)
{
    return pthread_getspecific(key);
}

static inline int tss_set(tss_t key, void *val)
{
    return (pthread_setspecific(key, val) == 0) ? thrd_success : thrd_error;
}

/* time */

int timespec_get_r(struct timespec *ts, int base)
{
    if (!ts) 
        return 0;
    if (base == TIME_UTC) {
        clock_gettime(CLOCK_REALTIME, ts);
        return base;
    } else if (base == TIME_MONO) {
        clock_gettime(CLOCK_MONOTONIC, ts);
        return base;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif  // defined(_WIN32) && !defined(__CYGWIN__)

#endif  // __THREADS_C11_H__

