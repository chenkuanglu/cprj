/**
 * @file    thrq.h
 * @author  ln
 * @brief   thread safe msg queue
 **/

#ifndef __THR_QUEUE__
#define __THR_QUEUE__

#include <stdbool.h>
#include <errno.h>
#include <sys/queue.h>
#include <pthread.h>
#include "mpool.h"
#include "mux.h"

#ifdef __cplusplus
extern "C" {
#endif

#define THRQ_MAX_SIZE_DEFAULT           10000

/**
 * declare user data type with list head struct: 
 *
 *  struct __thrq_elm {
 *      struct {
 *          struct __thrq_elm *next;
 *          struct __thrq_elm **prev;   // a pointer to pointer!
 *      } entry;
 *      // user data (empty as it is lib definition)
 *  } thrq_elm_t;
 *
 *    elm->entry.next  == pointer to next elm
 *    elm->entry.prev  == pointer to pre-elm's entry.next
 *  *(elm->entry.prev) == pointer to itself
 *
 * 'thrq_elm_t' is used as the base struct of user custom type.
 **/
typedef struct __thrq_elm {
    TAILQ_ENTRY(__thrq_elm) entry;
    int                     len;
    unsigned char           data[];     /* flexible array */
} thrq_elm_t;

/**
 * declare queue head struct with first & last pointer to the type of 'struct __thrq_elm': 
 *
 *  struct __thrq_head {
 *      struct __thrq_elm *first;   // pointer to first elm
 *      struct __thrq_elm **last;   // pointer to last-elm's entry.next
 *  };
 **/
typedef TAILQ_HEAD(__thrq_head, __thrq_elm) thrq_head_t;

/* thread safe queue control block */
typedef struct {
    mpool_t             mpool;
    
    thrq_head_t         head;           /* list header */
    mux_t               lock;           /* data lock */
    pthread_condattr_t  cond_attr;
    pthread_cond_t      cond;

    int                 count;
    int                 max_size;
} thrq_cb_t;

#define THRQ_BLOCK_SIZE(data_size)      (sizeof(thrq_elm_t) + (data_size))

#define THRQ_INIT(q)                    thrq_init(q)
#define THRQ_INIT_GROWN(q, s) \
    do { \
        thrq_init(q); \
        thrq_set_mpool(q, 0, s); \
    } while (0)

extern int          thrq_init           (thrq_cb_t *thrq);
extern thrq_cb_t*   thrq_new            (thrq_cb_t **thrq);
extern void         thrq_destroy        (thrq_cb_t *thrq);

extern int          thrq_set_maxsize    (thrq_cb_t *thrq, int max_size);
extern int          thrq_set_mpool      (thrq_cb_t *thrq, size_t n, size_t data_size);

extern bool         thrq_empty          (thrq_cb_t *thrq);
extern int          thrq_count          (thrq_cb_t *thrq);

extern int          thrq_send           (thrq_cb_t *thrq, void *data, int len);
extern int          thrq_receive        (thrq_cb_t *thrq, void *buf, int max_size, double timeout);

#ifdef __cplusplus
}
#endif

#endif /* __THR_QUEUE__ */

