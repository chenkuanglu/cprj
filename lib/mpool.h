/**
 * @file    mpool.h
 * @author  ln
 * @brief   memory pool managerment
 **/

#ifndef __MEMORY_POOL__
#define __MEMORY_POOL__

#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "mux.h"

#ifdef __cplusplus
extern "C" {
#endif 

enum {
    MPOOL_MODE_DESTROYED = 0,   /* invalid mode */
    MPOOL_MODE_MALLOC,          /* system malloc, mpool does nothing */
    MPOOL_MODE_DGROWN,          /* dynamic alloc & auto grown */
    MPOOL_MODE_ISTATIC,         /* internal static memory pool, mpool alloc/free the static buffer */
    MPOOL_MODE_ESTATIC          /* external memory pool, user alloc/free the static buffer */
};

typedef struct __mpool_elm {
    TAILQ_ENTRY(__mpool_elm)    entry;
    char                        data[];     /* flexible array */
} mpool_elm_t;

typedef TAILQ_HEAD(__mpool_head, __mpool_elm) mpool_head_t;

typedef struct __mpool mpool_t;
struct __mpool {
    mpool_head_t    hdr_free;   // list of block not used
    mpool_head_t    hdr_used;   // list of block used
    mux_t           lock;       // data lock
    size_t          data_size;  // data size (not block size)
    mpool_head_t    hdr_buf;    // list of dynamically buffer and need to free
    char*           sbuf;       // pointer to extern buffer
    int             mode;
};

#define MPOOL_BLOCK_NUM_ALLOC           128
#define MPOOL_BLOCK_SIZE(data_size)     (sizeof(mpool_elm_t) + data_size)

// init mpool according the mode
#define MPOOL_INIT_MALLOC(mpl)          mpool_init(mpl, 0, 0)
#define MPOOL_INIT(mpl)                 MPOOL_INIT_MALLOC(mpl)
#define MPOOL_INIT_GROWN(mpl, s)        mpool_init(mpl, 0, s)
#define MPOOL_INIT_ISTATIC(mpl, n, s)   mpool_init(mpl, n, s)
#define MPOOL_INIT_ESTATIC(mpl, buf, bs, ds) \
    do { \
        MPOOL_INIT_MALLOC(mpl); \
        mpool_setsbuf(mpl, buf, bs, ds); \
    } while (0)

extern int          mpool_init          (mpool_t *mpool, size_t n, size_t data_size);
extern mpool_t*     mpool_new           (size_t n, size_t data_size);
extern int          mpool_destroy       (mpool_t *mpool);

extern int          mpool_setsbuf       (mpool_t *mpool, char *buf, size_t buf_size, size_t data_size);

extern void*        mpool_malloc        (mpool_t *mpool, size_t size);
extern void         mpool_free          (mpool_t *mpool, void *mem);

#ifdef __cplusplus
}
#endif

#endif

