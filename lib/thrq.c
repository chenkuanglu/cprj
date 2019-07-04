/**
 * @file    thrq.h
 * @author  ln
 * @brief   通过队列在线程间发送与接收数据；发送的数据会被拷贝到队列暂存，直到数据被收取
 */

#include "thrq.h"
#include "err.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THRQ_EMPTY(thrq)        TAILQ_EMPTY(&thrq->head)
#define THRQ_FIRST(thrq)        TAILQ_FIRST(&thrq->head)

/**
 * @brief   初始化线程队列
 * @param   thrq    线程队列
 * @param   mp      内存池指针，当为NULL时，采用malloc和free
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int thrq_init(thrq_cb_t *thrq, mpool_t *mp)
{
    if (thrq == NULL) {
        errno = EINVAL;
        return -1;
    }

    TAILQ_INIT(&thrq->head);
    if (mux_init(&thrq->lock) != 0)
        return -1;
    pthread_condattr_init(&thrq->cond_attr);
    if ((errno = pthread_condattr_setclock(&thrq->cond_attr, CLOCK_MONOTONIC)) != 0)
        return -1;
    if ((errno = pthread_cond_init(&thrq->cond, &thrq->cond_attr) != 0))
        return -1;

    thrq->count = 0;
    thrq->mpool = mp;

    return 0;
}

 /**
 * @brief   创建线程队列
 *
 * @param   thrq    线程队列指针的指针
 * @param   mp      内存池指针，当为NULL时，采用malloc和free
 *
 * @return  返回新建的线程队列，并将该线程队列的指针赋给*thrq（如果thrq不为NULL的话）
 * @retval  !NULL   成功
 * @retval  NULL    失败并设置errno
 *
 * @par 示例：
 * @code
 * thrq_cb_t *thrq = thrq_new(NULL, NULL);
 * @endcode
 * 或者
 * @code
 * thrq_cb_t *thrq = NULL;
 * thrq_new(&thrq, NULL);   // thrq will not be NULL if success
 * @endcode
 *
 * @attention 返回的thrq对象需要free
 */
thrq_cb_t* thrq_new(thrq_cb_t **thrq, mpool_t *mp)
{
    thrq_cb_t *newq = (thrq_cb_t*)malloc(sizeof(thrq_cb_t));
    if (newq) {
        if (thrq_init(newq, mp) != 0) {
            free(newq);
            newq = NULL;
        }
    }

    if (thrq) {
        *thrq = newq;
    }
    return newq;
}

/**
 * @brief   队列是否为空，如果参数是NULL则“队列”始终为”空“
 * @param   thrq    线程队列指针
 * @retval  true    队列空
 * @retval  false   队列不空
 */
bool thrq_empty(thrq_cb_t *thrq)
{
    if (thrq == NULL) {
        return true;
    }
    mux_lock(&thrq->lock);
    bool empty = THRQ_EMPTY(thrq);
    mux_unlock(&thrq->lock);
    return empty;
}

/**
 * @brief   队列元素个数（或者有多少块数据）
 * @param   thrq    线程队列指针
 * @return  返回队列元素个数，如果参数为NULL，则返回0
 */
int thrq_count(thrq_cb_t *thrq)
{
    if (thrq == NULL) {
        return 0;
    }
    mux_lock(&thrq->lock);
    int count = thrq->count;
    mux_unlock(&thrq->lock);
    return count;
}

/**
 * @brief   内部函数，删除队列元素
 * @param   thrq    线程队列指针
 *          elm     要删除的元素
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
static int thrq_remove(thrq_cb_t *thrq, thrq_elm_t *elm)
{
    if (thrq == NULL || elm == NULL) {
        errno = EINVAL;
        return -1;
    }

    mux_lock(&thrq->lock);
    TAILQ_REMOVE(&thrq->head, elm, entry);
    if (thrq->mpool)
        mpool_free(thrq->mpool, elm);
    else
        free(elm);
    if (thrq->count > 0) {
        thrq->count--;
    }
    mux_unlock(&thrq->lock);
    return 0;
}

/**
 * @brief   内部函数，插入元素到队列
 * @param   thrq    线程队列指针
 *          data    插入的数据指针
 *          len     插入的数据长度
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
static int thrq_insert_tail(thrq_cb_t *thrq, void *data, size_t len)
{
    if (data == 0 || len == 0) {
        errno = EINVAL;
        return -1;
    }

    mux_lock(&thrq->lock);
    if (thrq->count >= THRQ_MAX_SIZE) {
        mux_unlock(&thrq->lock);
        errno = LIB_ERRNO_QUE_FULL;
        return -1;
    }

    thrq_elm_t *elm;
    if (thrq->mpool)
        elm = (thrq_elm_t*)mpool_malloc(thrq->mpool, THRQ_BLOCK_SIZE(len));
    else
        elm = (thrq_elm_t*)malloc(THRQ_BLOCK_SIZE(len));

    if (elm == 0) {
        int ec = errno;             // backup errno
        mux_unlock(&thrq->lock);    // errno may be modified
        errno = ec;
        return -1;
    }
    memcpy(elm->data, data, len);
    elm->len = len;
    TAILQ_INSERT_TAIL(&thrq->head, elm, entry);
    thrq->count++;

    mux_unlock(&thrq->lock);
    return 0;
}

/**
 * @brief   销毁线程队列
 * @param   thrq    线程队列指针
 * @return  void
 */
void thrq_destroy(thrq_cb_t *thrq)
{
    if (thrq) {
        mux_lock(&thrq->lock);
        pthread_cond_destroy(&thrq->cond);
        pthread_condattr_destroy(&thrq->cond_attr);
        while (!THRQ_EMPTY(thrq)) {
            thrq_remove(thrq, THRQ_FIRST(thrq));
        }
        thrq->mpool = NULL;
        mux_unlock(&thrq->lock);

        mux_destroy(&thrq->lock);
    }
}

/**
 * @brief   以阻塞或非阻塞方式发送队列消息
 * @param   thrq    线程队列指针
 *          data    发送的数据指针
 *          len     发送的数据长度
 *          flags   阻塞标志：0表示阻塞，THRQ_NOWAIT表示不阻塞（阻塞是指在加锁时锁被其他线程占用）
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int thrq_send(thrq_cb_t *thrq, void *data, size_t len, int flags)
{
    if (thrq == NULL || data == NULL || len == 0) {
        errno = EINVAL;
        return -1;
    }

    int lock_res;
    if (flags == THRQ_NOWAIT) {
        if ((lock_res = mux_trylock(&thrq->lock)) == EBUSY) {
            errno = lock_res;
            return -1;
        }
    } else {
        mux_lock(&thrq->lock);
    }
    if (thrq_insert_tail(thrq, data, len) != 0) {
        mux_unlock(&thrq->lock);
        return -1;
    }
    mux_unlock(&thrq->lock);

    int res;
    if ((res = pthread_cond_signal(&thrq->cond)) != 0) {
        errno = res;
        return -1;
    }
    return 0;
}

/**
 * @brief   以阻塞或非阻塞方式接收队列消息
 * @param   thrq        线程队列指针
 *          buf         接收缓存
 *          max_size    接收缓存的大小
 *          timeout     接收超时，单位为秒，0表示一直阻塞直到收到数据为止
 *          flags       阻塞标志：0表示阻塞，THRQ_NOWAIT表示不阻塞(即锁被占用或者无数据时立即返回)
 *
 * @return  成功返回实际收到的数据长度,失败返回-1并设置错误码
 *
 * @note    当函数收到数据或者发送错误时，将从阻塞状态返回；
 *          通过检查errno是否为 ETIMEDOUT 可以判断函数是否是超时返回
 * @par     举例：
 * @code
 * int num;
 * double tout = 3.0;
 * for (;;) {
 *     if ((num = thrq_receive(thrq, buf, bufsize, tout, 0)) == -1) {
 *         if (errno == ETIMEDOUT) {
 *             printf("receive timeout %.1fs\n", tout);
 *             continue;
 *         }
 *     }
 * }
 * @endcode
 */
int thrq_receive(thrq_cb_t *thrq, void *buf, size_t bufsize, double timeout, int flags)
{
    if (thrq == NULL || buf == NULL || bufsize == 0) {
        errno = EINVAL;
        return -1;
    }

    int res = 0;
    struct timespec ts;
    if (timeout > 0) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        // ok, max_long_int = 2.1s > (1s + 1s)
        ts.tv_nsec = (long)((timeout - (long)timeout) * 1000000000L) + ts.tv_nsec;
        ts.tv_sec = (time_t)timeout + ts.tv_sec + (ts.tv_nsec / 1000000000L);
        ts.tv_nsec = ts.tv_nsec % 1000000000L;
    }

    int lock_res;
    if (flags == THRQ_NOWAIT) {
        if ((lock_res = mux_trylock(&thrq->lock)) == EBUSY) {
            errno = lock_res;
            return -1;
        }
    } else {
        mux_lock(&thrq->lock);
    }

    if (flags != THRQ_NOWAIT) {
        /* break when error occured or data receive */
        while (res == 0 && thrq->count == 0) {
            if (timeout > 0) {
                res = pthread_cond_timedwait(&thrq->cond, &thrq->lock.mux, &ts);
            } else {
                res = pthread_cond_wait(&thrq->cond, &thrq->lock.mux);
            }
        }
        if (res != 0) {
            mux_unlock(&thrq->lock);
            errno = res;
            return -1;      // errno may be the ETIMEDOUT
        }
    } else {
        if (thrq->count == 0) {
            errno = LIB_ERRNO_QUE_EMPTY;
            mux_unlock(&thrq->lock);
            return -1;
        }
    }

    thrq_elm_t *elm = THRQ_FIRST(thrq);
    size_t cpsize = (bufsize < elm->len) ? bufsize : elm->len;
    memcpy(buf, elm->data, cpsize);
    thrq_remove(thrq, elm);

    mux_unlock(&thrq->lock);
    return cpsize;
}

#ifdef __cplusplus
}
#endif

