/**
 * @file    que.h
 * @author  ln
 * @brief   队列或者链表
 */

#ifndef __QUEUE__
#define __QUEUE__

#include <stdbool.h>
#include "sysque.h"
#include "threads_c11.h"
#include "err.h"
#include "mpool.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 对内存的使用做限制，避免内存池在自增长模式下向系统无限申请内存
#define QUE_MAX_SIZE            65536

/**
 * 收发的数据在队列里构成了一个链表，链表由 sys/queue.h 构造：
 * @code
 *  struct __que_elm {
 *      // list header
 *      struct {
 *          struct __que_elm *next;
 *          struct __que_elm **prev;    // a pointer to pointer!
 *      } entry;
 *
 *      // user data
 *      // ...
 *  } que_elm_t;
 * @endcode
 *
 * @note
 *
 *    elm->entry.next  : 指针，指向下一个元素
 *    elm->entry.prev  : 指针的指针，指向上一个元素所指向的下一个元素(prev's entry.next or itself)
 *  *(elm->entry.prev) : 对象，该语句表示元素自己
 */
typedef struct __que_elm {
    TAILQ_ENTRY(__que_elm)  entry;      ///< 链表元素的表头
    size_t                  len;        ///< 元素内的数据长度
    unsigned char           data[];     ///< 元素内的数据区
} que_elm_t;

typedef TAILQ_HEAD(__que_head, __que_elm) que_head_t;

/// 查找队列数据时，用于比较数据的回调函数
typedef int (*que_cmp_data_t)(const void*, const void*, size_t len);

/* queue control block */
typedef struct {
    mpool_t             *mpool;         ///< 内存池指针

    que_head_t          head;           ///< 数据队列
    mtx_t               lock;           ///< 互斥锁
    int                 count;          ///< 当前队列里的元素个数
} que_cb_t;

/// 队列是否空，非线程安全!
#define QUE_EMPTY(que)          TAILQ_EMPTY(&que->head)

/* first/begin element, not thread safe */
/// 队列首，非线程安全!
#define QUE_FIRST(que)          TAILQ_FIRST(&que->head)
/// 队列首，非线程安全!
#define QUE_BEGIN(que)          QUE_FIRST(que)
/// 队列尾，非线程安全!
#define QUE_LAST(que)           TAILQ_LAST(&que->head, __que_head)
/// 队列尾，非线程安全!
#define QUE_END(Thrq)           QUE_LAST(que)

/// 下一个元素，非线程安全!
#define QUE_NEXT(elm)           TAILQ_NEXT(elm, entry)
/// 上一个元素，非线程安全!
#define QUE_PREV(elm)           TAILQ_PREV(elm, __que_head, entry)

/**
 * @brief   正向for循环遍历，非线程安全!
 * @code
 * que_elm_t *var;
 * QUE_FOREACH(var, que) {
 *     memcpy(mybuf, var->data, var->len);
 * }
 * @endcode
 */
#define QUE_FOREACH(pelm, que) \
    TAILQ_FOREACH(pelm, &que->head, entry)
/// 反向for循环遍历，非线程安全!
#define QUE_FOREACH_REVERSE(pelm, que) \
    TAILQ_FOREACH_REVERSE(pelm, &que->head, __que_head, entry)

#define QUE_CONTAINER_OF(ptr) \
    ((que_elm_t *)((char *)(ptr) - ((size_t)&((que_elm_t *)0)->data)))

/// 通过数据区指针获取元素指针
#define QUE_DATA_ELM(data)              ( QUE_CONTAINER_OF(data) )
/// 通过元素指针获取数据区指针
#define QUE_ELM_DATA(elm, data_type)    ( (data_type *)((elm)->data) )

/// 队列元素的大小
#define QUE_BLOCK_SIZE(data_size)       (sizeof(que_elm_t) + (data_size))

#define QUE_LOCK(que)           mux_lock(&que->lock)
#define QUE_UNLOCK(que)         mux_unlock(&que->lock)

#define QUE_INIT(q)             que_init(q,0)
#define QUE_INIT_MP(q,m)        que_init(q,m)

/* thread safe */
extern int          que_init(que_cb_t *que, mpool_t *mp);
extern que_cb_t*    que_new(que_cb_t **que, mpool_t *mp);
extern void         que_destroy(que_cb_t *que);

extern bool         que_empty(que_cb_t *que);
extern int          que_count(que_cb_t *que);

extern int          que_insert_head(que_cb_t *que, void *data, size_t len);
extern int          que_insert_tail(que_cb_t *que, void *data, size_t len);

extern int          que_remove(que_cb_t *que, void *data, size_t len, que_cmp_data_t pfn_cmp);

/* not thread safe */
extern que_elm_t*   QUE_FIND(que_cb_t *que, void *data, size_t len, que_cmp_data_t pfn_cmp);
extern int          QUE_REMOVE(que_cb_t *que, que_elm_t *elm);
extern int          QUE_INSERT_AFTER(que_cb_t *que, que_elm_t *elm, void *data, size_t len);
extern int          QUE_INSERT_BEFORE(que_cb_t *que, que_elm_t *elm, void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __QUEUE__ */
