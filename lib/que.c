/**
 * @file    que.c
 * @author  ln
 * @brief   队列或者链表
 */

#include "que.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   初始化队列
 * @param   que 队列指针
 * @return  成功返回0，失败返回-1并设置errno
 */
int que_init(que_cb_t *que, mpool_t *mp)
{
    if (que == NULL) {
        errno = EINVAL;
        return -1;
    }
    TAILQ_INIT(&que->head);
    mtx_init(&que->lock, mtx_plain | mtx_recursive);
    que->count = 0;
    que->mpool = mp;
    return 0;
}

/**
 * @brief   创建队列
 * @param   que    队列指针
 * @return  返回新建的队列，并将该队列的指针赋给*que（如果que不为NULL的话）
 * @retval  !NULL   成功
 * @retval  NULL    失败并设置errno
 */
que_cb_t* que_new(que_cb_t **que, mpool_t *mp)
{
    que_cb_t *newq = (que_cb_t*)malloc(sizeof(que_cb_t));
    if (newq) {
        if (que_init(newq, mp) < 0) {
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
 * @brief   队列是否为空，如果参数是NULL则“队列”始终为”空“
 * @param   thrq    队列指针
 * @retval  true    队列空
 * @retval  false   队列不空
 */
bool que_empty(que_cb_t *que)
{
    if (que == NULL) {
        return true;
    }
    mtx_lock(&que->lock);
    int empty = QUE_EMPTY(que);
    mtx_unlock(&que->lock);

    return empty;
}

/**
 * @brief   队列元素个数（或者有多少块数据）
 * @param   thrq    队列指针
 * @return  返回队列元素个数，如果参数为NULL，则返回0
 */
int que_count(que_cb_t *que)
{
    if (que == NULL) {
        return 0;
    }
    mtx_lock(&que->lock);
    int count = que->count;
    mtx_unlock(&que->lock);

    return count;
}

/**
 * @brief   插入元素到队列首
 * @param   thrq    线程队列指针
 *          data    插入的数据指针
 *          len     插入的数据长度
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int que_insert_head(que_cb_t *que, void *data, size_t len)
{
    /* one byte data at least */
    if (que == 0 || data == 0 || len == 0) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&que->lock);
    /* queue is full */
    if (que->count >= QUE_MAX_SIZE) {
        mtx_unlock(&que->lock);
        errno = LIB_ERRNO_QUE_FULL;
        return -1;
    }
    /* malloc */
    que_elm_t *elm;
    if (que->mpool)
        elm = (que_elm_t*)mpool_malloc(que->mpool, QUE_BLOCK_SIZE(len));
    else
        elm = (que_elm_t*)malloc(QUE_BLOCK_SIZE(len));
    if (elm == 0) {
        mtx_unlock(&que->lock);
        return -1;
    }
    /* insert queue */
    memcpy(elm->data, data, len);
    elm->len = len;
    TAILQ_INSERT_HEAD(&que->head, elm, entry);
    que->count++;

    mtx_unlock(&que->lock);
    return 0;
}

/**
 * @brief   插入元素到队列尾
 * @param   thrq    线程队列指针
 *          data    插入的数据指针
 *          len     插入的数据长度
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int que_insert_tail(que_cb_t *que, void *data, size_t len)
{
    if (que == 0 || data == 0 || len == 0) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&que->lock);
    /* queue is full */
    if (que->count >= QUE_MAX_SIZE) {
        mtx_unlock(&que->lock);
        return -1;
    }
    /* malloc */
    que_elm_t *elm;
    if (que->mpool)
        elm = (que_elm_t*)mpool_malloc(que->mpool, QUE_BLOCK_SIZE(len));
    else
        elm = (que_elm_t*)malloc(QUE_BLOCK_SIZE(len));
    if (elm == 0) {
        mtx_unlock(&que->lock);
        return -1;
    }
    /* insert queue */
    memcpy(elm->data, data, len);
    elm->len = len;
    TAILQ_INSERT_TAIL(&que->head, elm, entry);
    que->count++;

    mtx_unlock(&que->lock);
    return 0;
}

/**
 * @brief   在某元素之后插入数据，非线程安全!
 * @param   que         队列
 *          list_elm    在该元素之后插入数据
 *          data        插入的数据
 *          len         插入的数据长度
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int QUE_INSERT_AFTER(que_cb_t *que, que_elm_t *list_elm, void *data, size_t len)
{
    if (list_elm == 0 || data == 0 || len == 0) {
        errno = EINVAL;
        return -1;
    }

    if (que->count >= QUE_MAX_SIZE) {
        errno = LIB_ERRNO_QUE_FULL;
        return -1;
    }
    que_elm_t *elm;
    if (que->mpool)
        elm = (que_elm_t*)mpool_malloc(que->mpool, QUE_BLOCK_SIZE(len));
    else
        elm = (que_elm_t*)malloc(QUE_BLOCK_SIZE(len));
    if (elm == 0) {
        return -1;
    }
    memcpy(elm->data, data, len);
    elm->len = len;
    TAILQ_INSERT_AFTER(&que->head, list_elm, elm, entry);
    que->count++;

    return 0;
}

/**
 * @brief   在某元素之后插入数据，非线程安全!
 * @param   que         队列
 *          list_elm    在该元素之前插入数据
 *          data        插入的数据
 *          len         插入的数据长度
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int QUE_INSERT_BEFORE(que_cb_t *que, que_elm_t *list_elm, void *data, size_t len)
{
    if (list_elm == 0 || data == 0 || len == 0) {
        errno = EINVAL;
        return -1;
    }

    if (que->count >= QUE_MAX_SIZE) {
        errno = LIB_ERRNO_QUE_FULL;
        return -1;
    }
    que_elm_t *elm;
    if (que->mpool)
        elm = (que_elm_t*)mpool_malloc(que->mpool, QUE_BLOCK_SIZE(len));
    else
        elm = (que_elm_t*)malloc(QUE_BLOCK_SIZE(len));
    if (elm == 0) {
        return -1;
    }
    memcpy(elm->data, data, len);
    elm->len = len;
    TAILQ_INSERT_BEFORE(list_elm, elm, entry);
    que->count++;

    return 0;
}

/**
 * @brief   销毁队列
 * @param   thrq    队列指针
 * @return  void
 */
void que_destroy(que_cb_t *que)
{
    if (que) {
        mtx_lock(&que->lock);
        while (!QUE_EMPTY(que)) {
            QUE_REMOVE(que, QUE_FIRST(que));
        }
        que->mpool = NULL;
        mtx_unlock(&que->lock);

        mtx_destroy(&que->lock);
    }
}

/**
 * @brief   删除队列元素
 * @param   que     队列指针
 *          elm     要删除的元素
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int QUE_REMOVE(que_cb_t *que, que_elm_t *elm)
{
    if (que == 0 || elm == 0) {
        errno = EINVAL;
        return -1;
    }
    TAILQ_REMOVE(&que->head, elm, entry);
    if (que->mpool)
        mpool_free(que->mpool, elm);
    else
        free(elm);
    if (que->count > 0) {
        que->count--;
    }
    return 0;
}

/**
 * @brief   删除队列元素
 * @param   que     队列指针
 *          data    要删除的数据
 *          len     要删除的数据长度
 *          pfn_cmp 数据比较函数，如果为NULL则按内存比较
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int que_remove(que_cb_t *que, void *data, size_t len, que_cmp_data_t pfn_cmp)
{
    if (que == 0 || data == 0 || len == 0) {
        errno = EINVAL;
        return -1;
    }
    que_elm_t *elm;
    mtx_lock(&que->lock);
    if ((elm = QUE_FIND(que, data, len, pfn_cmp)) == NULL) {
        errno = LIB_ERRNO_NOT_EXIST;
        mtx_unlock(&que->lock);
        return -1;
    }
    TAILQ_REMOVE(&que->head, elm, entry);
    if (que->mpool)
        mpool_free(que->mpool, elm);
    else
        free(elm);
    if (que->count > 0) {
        que->count--;
    }
    mtx_unlock(&que->lock);
    return 0;
}

/**
 * @brief   查找数据
 * @param   que     队列指针
 *          data    要查找的数据
 *          len     要查找的数据长度
 *          pfn_cmp 数据比较函数，如果为NULL则按内存比较
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
que_elm_t* QUE_FIND(que_cb_t *que, void *data, size_t len, que_cmp_data_t pfn_cmp)
{
    if (que == 0 || data == 0 || len == 0) {
        errno = EINVAL;
        return NULL;
    }

    if (pfn_cmp == NULL)
        pfn_cmp = memcmp;
    que_elm_t *var;
    QUE_FOREACH_REVERSE(var, que) {
        size_t cmp_size = len < var->len ? len : var->len;
        if (pfn_cmp(var->data, data, cmp_size) == 0) {
            return var;
        }
    }
    errno = LIB_ERRNO_NOT_EXIST;
    return NULL;
}

#ifdef __cplusplus
}
#endif
