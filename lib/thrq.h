/**
 * @file    thrq.h
 * @author  ln
 * @brief   通过队列在线程间发送与接收数据；发送的数据会被拷贝到队列暂存，直到数据被收取
 */

#ifndef __THR_QUEUE__
#define __THR_QUEUE__

#include <stdbool.h>
#include "sysque.h"
#include "threads_c11.h"
#include "err.h"
#include "mpool.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 对内存的使用做限制，避免内存池在自增长模式下向系统无限申请内存
#define THRQ_MAX_SIZE           65536

/**
 * 收发的数据在队列里构成了一个链表，链表由 sys/queue.h 构造：
 * @code
 *  struct __thrq_elm {
 *      // list header
 *      struct {
 *          struct __thrq_elm *next;
 *          struct __thrq_elm **prev;   // a pointer to pointer!
 *      } entry;
 *
 *      // user data
 *      // ...
 *  } thrq_elm_t;
 * @endcode
 *
 * @note
 *
 *    elm->entry.next  : 指针，指向下一个元素
 *    elm->entry.prev  : 指针的指针，指向上一个元素所指向的下一个元素(prev's entry.next or itself)
 *  *(elm->entry.prev) : 对象，该语句表示元素自己
 */
typedef struct __thrq_elm {
    TAILQ_ENTRY(__thrq_elm) entry;      ///< 链表元素的表头
    size_t                  len;        ///< 元素内的数据长度
    unsigned char           data[];     ///< 元素内的数据区
} thrq_elm_t;

typedef TAILQ_HEAD(__thrq_head, __thrq_elm) thrq_head_t;

/* the queue control block */
typedef struct {
    mpool_t             *mpool;         ///< 内存池指针

    thrq_head_t         head;           ///< 数据队列
    mtx_t               lock;           ///< 互斥锁
    cnd_t               cond;           ///< 条件变量

    int                 count;          ///< 当前队列里的元素个数
} thrq_cb_t;

#define THRQ_BLOCK_SIZE(data_size)      (sizeof(thrq_elm_t) + (data_size))

#define THRQ_INIT(q)                    thrq_init(q,0)
#define THRQ_INIT_MP(q,m)               thrq_init(q,m)

#define THRQ_NOWAIT                     1

extern int          thrq_init(thrq_cb_t *thrq, mpool_t *mp);
extern thrq_cb_t*   thrq_new(thrq_cb_t **thrq, mpool_t *mp);
extern void         thrq_destroy(thrq_cb_t *thrq);

extern bool         thrq_empty(thrq_cb_t *thrq);
extern int          thrq_count(thrq_cb_t *thrq);

extern int          thrq_send(thrq_cb_t *thrq, void *data, size_t len, int flags);
extern int          thrq_receive(thrq_cb_t *thrq, void *buf, size_t bufsize, double timeout, int flags);

#ifdef __cplusplus
}
#endif

#endif /* __THR_QUEUE__ */

