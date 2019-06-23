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
 *
 * @param   mux     未初始化的互斥锁
 *
 * @retval  0   成功
 * @retval  -1  失败并设置errno
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

    return pthread_mutex_init(&mux->mux, &mux->attr);
}

/**
 * @brief   创建一个线程之间的、优先级继承的、可嵌套的互斥锁
 *
 * @param   mux     互斥锁指针的指针
 
 * @return  返回新建的互斥锁，并将该互斥锁赋给*mux（如果mux不为NULL的话）
 * @retval  !NULL   成功
 * @retval  NULL    失败并设置errno
 *
 * @par 示例：
 * @code
 * mux_t *mux = mux_new(NULL);
 * @endcode
 * 或者
 * @code
 * mux_t *mux = NULL;
 * mux_new(&mux);
 * @endcode
 *
 * @attention 返回的互斥锁需要free，mux_destroy并不能free互斥锁本身
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
 *
 * @param   mux     互斥锁指针
 *
 * @return  void
 *
 * @attention mux_destroy并不能free由mux_new返回的动态内存
 *
 * @par 示例：
 * @code
 * mux_t *mux = NULL;
 * mux_new(&mux);
 * //...   
 * mux_destroy(mux);
 * free(mux);
 * mux = NULL;
 * @endcode
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
 *
 * @param   mux     互斥锁指针
 *
 * @retval  0   成功
 * @retval  !0  错误码
 */
int mux_lock(mux_t *mux)
{
    return pthread_mutex_lock(&mux->mux);
}

/**
 * @brief   解锁互斥锁
 *
 * @param   mux     互斥锁指针
 *
 * @retval  0   成功
 * @retval  !0  错误码
 */
int mux_unlock(mux_t *mux)
{
    return pthread_mutex_unlock(&mux->mux);
}

#ifdef __cplusplus
}
#endif

