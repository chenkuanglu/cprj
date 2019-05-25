/**
 * @file    que.h
 * @author  ln
 * @brief   queue or list
 **/

#ifndef __QUEUE__
#define __QUEUE__

#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include "mpool.h"
#include "mux.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QUE_MAX_SIZE_DEFAULT            10000

/**
 * declare user data type with list head struct: 
 *
 *  struct __que_elm {
 *      struct {
 *          struct __que_elm *next;
 *          struct __que_elm **prev;   // a pointer to pointer!
 *      } entry;
 *      // user data (empty as it is lib definition)
 *  } que_elm_t;
 *
 *    elm->entry.next  == pointer to next elm
 *    elm->entry.prev  == pointer to pre-elm's entry.next
 *  *(elm->entry.prev) == pointer to itself
 *
 * 'que_elm_t' is used as the base struct of user custom type.
 **/
typedef struct __que_elm {
    TAILQ_ENTRY(__que_elm)  entry;
    int                     len;
    unsigned char           data[];     /* flexible array */
} que_elm_t;

/**
 * declare queue head struct with first & last pointer to the type of 'struct __que_elm': 
 *
 *  struct __que_head {
 *      struct __que_elm *first;   // pointer to first elm
 *      struct __que_elm **last;   // pointer to last-elm's entry.next
 *  };
 **/
typedef TAILQ_HEAD(__que_head, __que_elm) que_head_t;

typedef int     (*que_cmp_data_t)(const void*, const void*, size_t len);

/* thread safe queue control block */
typedef struct {
    mpool_t             mpool;

    que_head_t          head;           /* list header */
    mux_t               lock;           /* data lock */
    int                 count;
    int                 max_size;
} que_cb_t;

/* is empty, not thread safe */  
#define QUE_EMPTY(que)          TAILQ_EMPTY(&que->head)

/* first/begin element, not thread safe */
#define QUE_FIRST(que)          TAILQ_FIRST(&que->head)
#define QUE_BEGIN(que)          QUE_FIRST(que)
/* last/end element, not thread safe */
#define QUE_LAST(que)           TAILQ_LAST(&que->head, __que_head)
#define QUE_END(Thrq)           QUE_LAST(que)

/* element's next one, not thread safe */
#define QUE_NEXT(elm)           TAILQ_NEXT(elm, entry)
/* element's previous one, not thread safe */
#define QUE_PREV(elm)           TAILQ_PREV(elm, __que_head, entry)

/* for each element, not thread safe */
#define QUE_FOREACH(pelm, que) \
    TAILQ_FOREACH(pelm, &que->head, entry)
/* for each element reversely, not thread safe */
#define QUE_FOREACH_REVERSE(pelm, que) \
    TAILQ_FOREACH_REVERSE(pelm, &que->head, __que_head, entry)

#define QUE_CONTAINER_OF(ptr)           ((que_elm_t *)((char *)(ptr) - ((size_t)&((que_elm_t *)0)->data)))

/* parse element/data pointer to data/element pointer */
#define QUE_DATA_ELM(data)              ( QUE_CONTAINER_OF(data) )
#define QUE_ELM_DATA(elm, data_type)    ( *((data_type *)((elm)->data)) )

#define QUE_BLOCK_SIZE(data_size)       (sizeof(que_elm_t) + (data_size))

/* lock/unlock queue, thread safe */
#define QUE_LOCK(que)          mux_lock(&que->lock)
#define QUE_UNLOCK(que)        mux_unlock(&que->lock)

#define QUE_INIT(q)            que_init(q)
#define QUE_INIT_GROWN(q, s) \
    do { \
        que_init(q); \
        que_set_mpool(q, 0, s); \
    } while (0)

/* thread safe */
extern int          que_init            (que_cb_t *que);
extern que_cb_t*    que_new             (que_cb_t **que);
extern void         que_destroy         (que_cb_t *que);

extern int          que_set_maxsize     (que_cb_t *que, int max_size);
extern int          que_set_mpool       (que_cb_t *que, size_t n, size_t data_size);

extern bool         que_empty           (que_cb_t *que);
extern int          que_count           (que_cb_t *que);

extern int          que_insert_head     (que_cb_t *que, void *data, int len);
extern int          que_insert_tail     (que_cb_t *que, void *data, int len);

/* not thread safe */
extern que_elm_t*   QUE_FIND            (que_cb_t *que, void *data, int len, que_cmp_data_t pfn_cmp);
extern int          QUE_REMOVE          (que_cb_t *que, que_elm_t *elm);
extern int          QUE_INSERT_AFTER    (que_cb_t *que, que_elm_t *elm, void *data, int len);
extern int          QUE_INSERT_BEFORE   (que_cb_t *que, que_elm_t *elm, void *data, int len);

#ifdef __cplusplus
}
#endif

#endif /* __QUEUE__ */

