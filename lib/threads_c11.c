/**
 * @file    threads_c11.c
 * @author  ln
 * @brief   implement of C11 <threads.h> emulation library
 */

#if !defined(_WIN32) || defined(__CYGWIN__)

#ifdef __cplusplus
extern "C" {
#endif

#include "threads_c11.h"

/*
 * EMULATED_THREADS_USE_NATIVE_TIMEDLOCK
 * Use pthread_mutex_timedlock() for `mtx_timedlock()'
 * Otherwise use mtx_trylock() + *busy loop* emulation.
 */
#if !defined(__CYGWIN__) && !defined(__APPLE__) && !defined(__NetBSD__)
#define EMULATED_THREADS_USE_NATIVE_TIMEDLOCK
#endif

struct impl_thrd_param {
    thrd_start_t func;
    void *arg;
};

static void* impl_thrd_routine(void *p)
{
    struct impl_thrd_param pack = *((struct impl_thrd_param *)p);
    free(p);
    return (void*)(intptr_t)pack.func(pack.arg);
}

void call_once(once_flag *flag, void (*func)(void))
{
    pthread_once(flag, func);
}

/* thread */

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
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

void thrd_exit(int res)
{
    pthread_exit((void*)(intptr_t)res);
}

int thrd_join(thrd_t thr, int *res)
{
    void *code;
    if (pthread_join(thr, &code) != 0)
        return thrd_error;
    if (res)
        *res = (int)(intptr_t)code;
    return thrd_success;
}

thrd_t thrd_current(void)
{
    return pthread_self();
}

int thrd_detach(thrd_t thr)
{
    return (pthread_detach(thr) == 0) ? thrd_success : thrd_error;
}

int thrd_equal(thrd_t thr0, thrd_t thr1)
{
    return pthread_equal(thr0, thr1);
}

void thrd_sleep(const struct timespec *time_point, struct timespec *remaining)
{
    if (time_point != NULL)
        nanosleep(time_point, remaining);
}

void thrd_yield(void)
{
    sched_yield();
}

/* condition variable */

int cnd_broadcast(cnd_t *cond)
{
    return (pthread_cond_broadcast(&cond->cnd) == 0) ? thrd_success : thrd_error;
}

void cnd_destroy(cnd_t *cond)
{
    pthread_cond_destroy(&cond->cnd);
    pthread_condattr_destroy(&cond->cnd_attr);
}

int __cnd_init(cnd_t *cond, int base)
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

int cnd_signal(cnd_t *cond)
{
    return (pthread_cond_signal(&cond->cnd) == 0) ? thrd_success : thrd_error;
}

// 设置超时参数时, timespec_get() 的参数必须是 TIME_MONO, 因为 cnd_init() 的默认值是 TIME_MONO
int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *abs_time)
{
    int rt = pthread_cond_timedwait(&cond->cnd, mtx, abs_time);
    if (rt == ETIMEDOUT)
        return thrd_busy;
    return (rt == 0) ? thrd_success : thrd_error;
}

int cnd_wait(cnd_t *cond, mtx_t *mtx)
{
    return (pthread_cond_wait(&cond->cnd, mtx) == 0) ? thrd_success : thrd_error;
}

/* mutex lock */

int mtx_init(mtx_t *mtx, int type)
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

void mtx_destroy(mtx_t *mtx)
{
    pthread_mutex_destroy(mtx);
}

int mtx_lock(mtx_t *mtx)
{
    return (pthread_mutex_lock(mtx) == 0) ? thrd_success : thrd_error;
}

int mtx_trylock(mtx_t *mtx)
{
    return (pthread_mutex_trylock(mtx) == 0) ? thrd_success : thrd_busy;
}

int mtx_timedlock(mtx_t *mtx, const struct timespec *ts)
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

int mtx_unlock(mtx_t *mtx)
{
    return (pthread_mutex_unlock(mtx) == 0) ? thrd_success : thrd_error;
}

/* thread-specific storage */

int tss_create(tss_t *key, tss_dtor_t dtor)
{
    if (key == NULL) {
        errno = EINVAL;
        return thrd_error;
    }
    return (pthread_key_create(key, dtor) == 0) ? thrd_success : thrd_error;
}

void tss_delete(tss_t key)
{
    pthread_key_delete(key);
}

void* tss_get(tss_t key)
{
    return pthread_getspecific(key);
}

int tss_set(tss_t key, void *val)
{
    return (pthread_setspecific(key, val) == 0) ? thrd_success : thrd_error;
}

/* time */

int timespec_get(struct timespec *ts, int base)
{
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

