/**
 * @file    que.c
 * @author  ln
 * @brief   msg queue
 **/

#include "que.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   init que control block
 * @param   que        queue to be init
 *
 * @return  0 is ok
 **/
int que_init(que_cb_t *que)
{
    if (que == NULL) {
        errno = EINVAL;
        return -1;
    }
    TAILQ_INIT(&que->head);
    mux_init(&que->lock);
    que->count      = 0;
    que->max_size   = QUE_MAX_SIZE_DEFAULT;
    if (mpool_init(&que->mpool, 0, 0) != 0)
        return -1;

    return 0;
}

/**
 * @brief   create que
 * @param   que    ponter to the queue-pointer
 * @return  return a pointer to the queue created
 *
 * example: que_cb_t *myq = que_create(0);
 *          or 
 *          que_cb_t *myq;
 *          que_create(&myq);
 **/
que_cb_t* que_new(que_cb_t **que)
{
    que_cb_t *newq = (que_cb_t*)malloc(sizeof(que_cb_t));
    if (newq) {
        if (que_init(newq) < 0) {
            free(newq);
            newq = NULL;
        }
    }

    if (que) {
        *que = newq;
    }
    return newq;
}

/**
 * @brief   clean & init memory pool for que alloc/free
 * @param   que         queue
 *          n           number of data element
 *          data_size   max size of user data
 *
 * @return  0 is ok
 **/
int que_set_mpool(que_cb_t *que, size_t n, size_t data_size)
{
    if (mux_lock(&que->lock) < 0)
        return -1;
    mpool_destroy(&que->mpool);
    if (mpool_init(&que->mpool, n, QUE_BLOCK_SIZE(data_size)) != 0) {
        mux_unlock(&que->lock);
        return -1;
    }
    mux_unlock(&que->lock);
    return 0;
}

/**
 * @brief   set max size of que
 * @param   que         queue
 *          max_size    >= count of elements 
 *
 * @return  0 is ok.
 **/
int que_set_maxsize(que_cb_t *que, int max_size)
{
    if (mux_lock(&que->lock) != 0)
        return -1;
    que->max_size = max_size;
    mux_unlock(&que->lock);
    return 0;
}

/**
 * @brief   is queue empty
 * @param   que    pointer to the queue
 * @return  true(!0) or false(0)
 **/
int que_empty(que_cb_t *que)
{
    if (mux_lock(&que->lock) < 0)
        return 1;   // true
    int empty = QUE_EMPTY(que);
    mux_unlock(&que->lock);

    return empty;
}

/**
 * @brief   get queue count
 * @param   que    pointer to the queue
 * @return  elments count
 **/
int que_count(que_cb_t *que)
{
    if (mux_lock(&que->lock) != 0)
        return -1;
    int count = que->count;
    mux_unlock(&que->lock);

    return count;
}

/**
 * @brief   insert element to the head
 * @param   que    queue to be insert
 *          data    the data to insert
 *          len     data length
 *
 * @return  0 is ok
 **/
int que_insert_head(que_cb_t *que, void *data, int len)
{
    /* one byte data at least */
    if (que == 0 || data == 0 || len == 0) {
        errno = EINVAL;
        return -1;
    }

    /* queue is full */
    mux_lock(&que->lock);

    if (que->count >= que->max_size) {        
        mux_unlock(&que->lock);
        errno = EAGAIN;
        return -1;
    }

    /* memoy allocate */
    que_elm_t *elm = (que_elm_t*)mpool_malloc(&que->mpool, QUE_BLOCK_SIZE(len));
    if (elm == 0) {
        mux_unlock(&que->lock);
        errno = ENOMEM;
        return -1;
    }

    /* insert queue */
    memcpy(elm->data, data, len);
    elm->len = len;
    TAILQ_INSERT_HEAD(&que->head, elm, entry);
    que->count++;

    mux_unlock(&que->lock);

    return 0;
}

/**
 * @brief   insert element to the tail
 * @param   que    queue to be insert
 *          data    the data to insert
 *          len     data length
 *
 * @return  0 is ok
 **/
int que_insert_tail(que_cb_t *que, void *data, int len)
{
    if (que == 0 || data == 0 || len == 0) {
        errno = EINVAL;
        return -1;
    }

    /* queue is full */
    mux_lock(&que->lock);

    if (que->count >= que->max_size) {        
        mux_unlock(&que->lock);
        return -1;
    }

    que_elm_t *elm = (que_elm_t*)mpool_malloc(&que->mpool, QUE_BLOCK_SIZE(len));
    if (elm == 0) {
        mux_unlock(&que->lock);
        errno = EAGAIN;
        return -1;
        return -1;
    }

    memcpy(elm->data, data, len);
    elm->len = len;
    TAILQ_INSERT_TAIL(&que->head, elm, entry);
    que->count++;

    mux_unlock(&que->lock);

    return 0;
}

/**
 * @brief   insert element after any queue elment
 * @param   que        queue to be insert
 *          list_elm    the queue elment which to insert after
 *          data        the data to insert
 *          len         data length
 *
 * @return  0 is ok
 **/
int QUE_INSERT_AFTER(que_cb_t *que, que_elm_t *list_elm, void *data, int len)
{
    if (list_elm == 0 || data == 0 || len == 0) {
        errno = EINVAL;
        return -1;
    }

    /* queue is full */
    if (que->count >= que->max_size) {        
        errno = EAGAIN;
        return -1;
    }

    que_elm_t *elm = (que_elm_t*)mpool_malloc(&que->mpool, QUE_BLOCK_SIZE(len));
    if (elm == 0) {
        errno = ENOMEM;
        return -1;
    }

    memcpy(elm->data, data, len);
    elm->len = len;
    TAILQ_INSERT_AFTER(&que->head, list_elm, elm, entry);
    que->count++;

    return 0;
}

/**
 * @brief   insert element before any queue elment
 * @param   que        queue to be insert
 *          list_elm    the queue elment which to insert before
 *          data        the data to insert
 *          len         data length
 *
 * @return  0 is ok
 **/
int QUE_INSERT_BEFORE(que_cb_t *que, que_elm_t *list_elm, void *data, int len)
{
    if (list_elm == 0 || data == 0 || len == 0) {
        errno = EINVAL;
        return -1;
    }

    /* queue is full */
    if (que->count >= que->max_size) {        
        errno = EAGAIN;
        return -1;
    }

    que_elm_t *elm = (que_elm_t*)mpool_malloc(&que->mpool, QUE_BLOCK_SIZE(len));
    if (elm == 0) {
        errno = ENOMEM;
        return -1;
    }

    memcpy(elm->data, data, len);
    elm->len = len;
    TAILQ_INSERT_BEFORE(list_elm, elm, entry);
    que->count++;

    return 0;
}

/**
 * @brief   free all the elements of que (except que itself)
 * @param   que    queue to clean
 * @return  void
 **/
void que_destroy(que_cb_t *que)
{
    if (que) {
        if (mux_lock(&que->lock) != 0)
            return;
        while (!QUE_EMPTY(que)) {
            QUE_REMOVE(que, QUE_FIRST(que));
        }    
        mux_unlock(&que->lock);

        mux_destroy(&que->lock);
        mpool_destroy(&que->mpool);
    }
}

/**
 * @brief   remove element
 * @param   que    ponter to the queue
 *          elm     the elment to be removed
 *
 * @return  0 is ok
 **/
int QUE_REMOVE(que_cb_t *que, que_elm_t *elm)
{
    if (que == 0 && elm == 0) {
        errno = EINVAL;
        return -1;
    }
    TAILQ_REMOVE(&que->head, elm, entry);
    mpool_free(&que->mpool, elm);
    if (que->count > 0) {
        que->count--;
    }
    return 0;
}

/**
 * @brief   concat 2 que
 * @param   que1   ponter to the queue 1
 *          que2   ponter to the queue 2
 *
 * @return  0 is ok
 *
 * que1 = que1 + que2, and que2 = init
 **/
int que_concat(que_cb_t *que1, que_cb_t *que2)
{
    if (mux_lock(&que1->lock) != 0)
        return -1;
    if (mux_lock(&que2->lock) != 0)
        return -1;
    TAILQ_CONCAT(&que1->head, &que2->head, entry);
    mux_unlock(&que2->lock);
    mux_unlock(&que1->lock);

    return 0;
}

/**
 * @brief   find element from queue
 * @param   que        queue to be insert
 *          data        the data to find
 *          len         data length
 *
 * @return  return the pointer to the element found
 **/
que_elm_t* QUE_FIND(que_cb_t *que, void *data, int len, que_cmp_data_t pfn_cmp)
{
    que_elm_t *var;
    QUE_FOREACH(var, que) {
        if (pfn_cmp(var->data, data, fmin(len, var->len)) == 0) {
            return var;
        }
    }    
    return 0;
}

#ifdef __cplusplus
}
#endif

