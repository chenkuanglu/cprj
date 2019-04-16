/**
 * @file    thrq.c
 * @author  ln
 * @brief   thread safe msg queue
 **/

#include "thrq.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <tgmath.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THRQ_EMPTY(thrq)        TAILQ_EMPTY(&thrq->head)
#define THRQ_FIRST(thrq)        TAILQ_FIRST(&thrq->head)

/**
 * @brief   init thrq control block
 * @param   thrq        queue to be init
 *
 * @return  0 is ok
 **/
int thrq_init(thrq_cb_t *thrq)
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

    thrq->count     = 0;
    thrq->max_size  = THRQ_MAX_SIZE_DEFAULT;

    if (mpool_init(&thrq->mpool, 0, 0) != 0)
        return -1;
    return 0;
}

/**
 * @brief   create thrq
 * @param   thrq    ponter to the queue-pointer
 * @return  return a pointer to the queue created
 *
 * example: thrq_cb_t *myq = thrq_create(0);
 *          or 
 *          thrq_cb_t *myq;
 *          thrq_create(&myq);
 **/
thrq_cb_t* thrq_new(thrq_cb_t **thrq)
{
    thrq_cb_t *newq = (thrq_cb_t*)malloc(sizeof(thrq_cb_t));
    if (newq) {
        if (thrq_init(newq) < 0) {
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
 * @brief   remove element
 * @param   thrq    ponter to the queue
 *          elm     the elment to be removed
 *
 * @return  0 is ok. 0 is also returned if thrq or elm is NULL.
 **/
static int thrq_remove(thrq_cb_t *thrq, thrq_elm_t *elm)
{
    if (thrq != 0 && elm != 0) {
        if (mux_lock(&thrq->lock) < 0)
            return -1;
        TAILQ_REMOVE(&thrq->head, elm, entry);
        mpool_free(&thrq->mpool, elm);
        if (thrq->count > 0) {
            thrq->count--;
        }
        mux_unlock(&thrq->lock);
    }
    return 0;
}

/**
 * @brief   free all the elements of thrq (except thrq itself)
 * @param   thrq    queue to clean
 * @return  void
 **/
void thrq_destroy(thrq_cb_t *thrq)
{
    if (thrq) {
        if (mux_lock(&thrq->lock) != 0)
            return;
        pthread_cond_destroy(&thrq->cond);
        pthread_condattr_destroy(&thrq->cond_attr);
        while (!THRQ_EMPTY(thrq)) {
            thrq_remove(thrq, THRQ_FIRST(thrq));
        }    
        mux_unlock(&thrq->lock);

        mux_destroy(&thrq->lock);
        mpool_destroy(&thrq->mpool);
    }
}

/**
 * @brief   clean & init memory pool for thrq alloc/free
 * @param   thrq        queue
 *          n           number of data element
 *          data_size   max size of user data
 *
 * @return  0 is ok
 **/
int thrq_set_mpool(thrq_cb_t *thrq, size_t n, size_t data_size)
{
    if (mux_lock(&thrq->lock) < 0)
        return -1;
    mpool_destroy(&thrq->mpool);
    if (mpool_init(&thrq->mpool, n, data_size) != 0) {
        mux_unlock(&thrq->lock);
        return -1;
    }
    mux_unlock(&thrq->lock);
    return 0;
}

/**
 * @brief   set max size of thrq
 * @param   thrq        queue
 *          max_size    >= count of elements 
 *
 * @return  0 is ok.
 **/
int thrq_set_maxsize(thrq_cb_t *thrq, int max_size)
{
    if (mux_lock(&thrq->lock) != 0)
        return -1;
    thrq->max_size = max_size;
    mux_unlock(&thrq->lock);
    return 0;
}

/**
 * @brief   is queue empty
 * @param   thrq    pointer to the queue
 * @return  true(!0) or false(0)
 **/
int thrq_empty(thrq_cb_t *thrq)
{
    if (mux_lock(&thrq->lock) < 0)
        return 1;   // true
    int empty = THRQ_EMPTY(thrq);
    mux_unlock(&thrq->lock);

    return empty;
}

/**
 * @brief   get queue count
 * @param   thrq    pointer to the queue
 * @return  elments count
 **/
int thrq_count(thrq_cb_t *thrq)
{
    if (mux_lock(&thrq->lock) < 0)
        return -1;
    int count = thrq->count;
    mux_unlock(&thrq->lock);

    return count;
}

/**
 * @brief   insert element to the tail
 * @param   thrq    queue to be insert
 *          data    the data to insert
 *          len     data length
 *
 * @return  0 is ok
 **/
static int thrq_insert_tail(thrq_cb_t *thrq, void *data, int len)
{
    if (data == 0 || len == 0) {
        errno = EINVAL;
        return -1;
    }

    /* queue is full */
    if (mux_lock(&thrq->lock) < 0)
        return -1;

    if (thrq->count >= thrq->max_size) {
        mux_unlock(&thrq->lock);
        errno = EAGAIN;
        return -1;
    }

    thrq_elm_t *elm = (thrq_elm_t*)mpool_malloc(&thrq->mpool, (sizeof(thrq_elm_t) + len));
    if (elm == 0) {
        mux_unlock(&thrq->lock);
        errno = ENOMEM;
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
 * @brief   insert element and send signal
 * @param   thrq    queue to be send
 *          data    the data to send
 *          len     data length
 *
 * @return  0 is ok
 **/
int thrq_send(thrq_cb_t *thrq, void *data, int len)
{
    if (mux_lock(&thrq->lock) < 0)
        return -1;
    if (thrq_insert_tail(thrq, data, len) != 0) {
        mux_unlock(&thrq->lock);
        return -1;
    }
    if (pthread_cond_signal(&thrq->cond) != 0) {
        return -1;
    }
    mux_unlock(&thrq->lock);

    return 0;
}

/**
 * @brief   receive and remove element
 * @param   thrq        queue to receive
 *          buf         the data buf
 *          max_size    buf size
 *          timeout     thread block time, 0 is block until signal received
 *
 * @return  0 is ok, -VALUE is error.
 * 
 *  function returns when error occured or data received,
 *  -ETIMEDOUT returned while timeout (ETIMEDOUT defined in <errno.h>)
 **/
int thrq_receive(thrq_cb_t *thrq, void *buf, int max_size, double timeout)
{
    int res = 0;
    struct timespec ts;

    if (timeout > 0) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_nsec = (long)((timeout - (long)timeout) * 1000000000L) + ts.tv_nsec;  // ok, max_long_int = 2.1s > (1s + 1s)
        ts.tv_sec = (time_t)timeout + ts.tv_sec + (ts.tv_nsec / 1000000000L);
        ts.tv_nsec = ts.tv_nsec % 1000000000L;
    }

    if (mux_lock(&thrq->lock) != 0)
        return -1;

    /* break when error occured or data receive */
    while (res == 0 && thrq->count == 0) {
        if (timeout > 0) {
            res = pthread_cond_timedwait(&thrq->cond, &thrq->lock.mux, &ts);
        } else {
            res = pthread_cond_wait(&thrq->cond, &thrq->lock.mux);
        }
    }
    if (res != 0) {
        const int err = res;
        mux_unlock(&thrq->lock);
        errno = err;
        return -1;    // errno may be ETIMEDOUT
    }

    /* data received */
    if (THRQ_EMPTY(thrq)) {
        mux_unlock(&thrq->lock);    // critical error !!!
        return -1;
    }
    thrq_elm_t *elm = THRQ_FIRST(thrq);
    res = (max_size < elm->len) ? max_size : elm->len;
    memcpy(buf, elm->data, res);
    thrq_remove(thrq, elm);

    mux_unlock(&thrq->lock);
    return res;
}

#ifdef __cplusplus
}
#endif

