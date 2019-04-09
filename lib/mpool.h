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
    MPOOL_MODE_DESTROY = 0,     /* invalid mode */
    MPOOL_MODE_MALLOC,          /* system malloc, mpool does nothing */
    MPOOL_MODE_DGROWN,          /* dynamic alloc & auto grown & internal buffer is NULL */
    MPOOL_MODE_ISTATIC,         /* internal static memory pool, mpool alloc/free the static buffer */
    MPOOL_MODE_ESTATIC          /* extern memory pool, user alloc/free the static buffer */
};

typedef struct __mpool_elm {
    TAILQ_ENTRY(__mpool_elm)    entry;
    char                        data[];     /* flexible array */
} mpool_elm_t;

typedef TAILQ_HEAD(__mpool_head, __mpool_elm) mpool_head_t;

typedef struct __mpool mpool_t;
struct __mpool {
    mpool_head_t    hdr_free;
    mpool_head_t    hdr_used;
    mux_t           lock;
    size_t          data_size;
    char*           buffer;
    int             mode;
};

#define MPOOL_BLOCK_SIZE(data_size)     (sizeof(mpool_elm_t) + data_size)

extern int          mpool_init          (mpool_t *mpool, size_t n, size_t data_size);
extern mpool_t*     mpool_new           (size_t n, size_t data_size);
extern int          mpool_destroy       (mpool_t *mpool);

extern int          mpool_setbuf        (mpool_t *mpool, char *buf, size_t buf_size, size_t data_size);

extern void*        mpool_malloc        (mpool_t *mpool, size_t size);
extern void         mpool_free          (mpool_t *mpool, void *mem);

#ifdef __cplusplus
}
#endif

#endif

