/**
 * @file    mux.c
 * @author  ln
 * @brief   创建并使用一个线程之间的、优先级继承的、可嵌套的互斥锁
 */

#include "mux.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   初始化一个线程之间的、优先级继承的、可嵌套的互斥锁
 * @param   mutex   未初始化的互斥锁
 * @return  成功返回0，失败返回-1并设置errno
 *
 * 举例：
 * mux_t *mux = mux_new(NULL);
 * or
 * mux_t *mux = NULL;
 * mux_new(&mux);
 */
int mux_init(mux_t *mux)
{
    if (mux == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    pthread_mutexattr_init(&mux->attr);
    if ((errno = pthread_mutexattr_setpshared(&mux->attr, PTHREAD_PROCESS_PRIVATE)) != 0) 
        return -1;
    if ((errno = pthread_mutexattr_setprotocol(&mux->attr, PTHREAD_PRIO_INHERIT)) != 0) 
        return -1;
    if ((errno = pthread_mutexattr_settype(&mux->attr, PTHREAD_MUTEX_RECURSIVE)) != 0) 
        return -1;

    pthread_mutex_init(&mux->mux, &mux->attr);  ///< always returns 0.
    return 0;
}

/**
 * @brief   创建一个线程之间的、优先级继承的、可嵌套的互斥锁
 * @param   mutex   互斥锁指针的指针
 * @return  成功返回新建的互斥锁指针，失败返回NULL并设置errno
 *
 * 举例：
 * mux_t *mux = mux_new(NULL);
 * or
 * mux_t *mux = NULL;
 * mux_new(&mux);
 */
mux_t* mux_new(mux_t **mux)
{
    mux_t *p = (mux_t *)malloc(sizeof(mux_t));
    if (p && (mux_init(p) != 0)) {
        free(p);
        p = NULL;
    }

    if (mux != NULL)
        *mux = p;
    return p;
}

/**
 * @brief   销毁互斥锁
 * @param   mutex   互斥锁指针
 * @return  void
 */
void mux_destroy(mux_t *mux)
{
    if (mux) {
        pthread_mutexattr_destroy(&mux->attr);
        pthread_mutex_destroy(&mux->mux);
    }
}

/**
 * @brief   加锁互斥锁
 * @param   mutex   互斥锁指针
 * @return  成功返回0，失败返回非0错误码
 */
int mux_lock(mux_t *mux)
{
    return pthread_mutex_lock(&mux->mux);
}

/**
 * @brief   解锁互斥锁
 * @param   mutex   互斥锁指针
 * @return  成功返回0，失败返回非0错误码
 */
int mux_unlock(mux_t *mux)
{
    return pthread_mutex_unlock(&mux->mux);
}

#ifdef __cplusplus
}
#endif

