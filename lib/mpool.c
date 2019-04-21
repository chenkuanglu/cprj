/**
 * @file    mpool.c
 * @author  ln
 * @brief   memory pool managerment
 **/

#include "mpool.h"

#ifdef __cplusplus
extern "C" {
#endif 

#define OFFSET_OF(TYPE, MEMBER)             ((size_t)&((TYPE *)0)->MEMBER)
#define CONTAINER_OF(ptr, type, member)     ((type *)((char *)(ptr) - OFFSET_OF(type,member)))

/**
 * @brief   Init memory pool, do not init a mpool in use before clean it,
 *          otherwise memory leaks
 * @param   mpool       memory poll
 *          n           number of data element
 *          data_size   max size of user data
 *
 * @return  0 is ok
 *
 * MPOOL_MODE_DESTROY: mpool been cleaned
 * mpool not available
 *
 * MPOOL_MODE_MALLOC: data_size = 0
 * use system malloc & free, mpool does nothing
 *
 * MPOOL_MODE_DGROWN: n = 0, data_size != 0
 * malloc every element, but never free except mpool deleted
 *
 * MPOOL_MODE_ISTATIC: n != 0, data_size != 0
 * mpool once malloc/free a big pool for all n*elements
 *
 * MPOOL_MODE_ESTATIC: call the function 'mpool_setbuf()'   
 * extern caller give/retrieve a big pool for all elements
 **/
int mpool_init(mpool_t *mpool, size_t n, size_t data_size)
{
    if (mpool == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (data_size == 0) {
        mpool->mode = MPOOL_MODE_MALLOC;
    } else {
        if (n == 0)
            mpool->mode = MPOOL_MODE_DGROWN;
        else
            mpool->mode = MPOOL_MODE_ISTATIC;
    }
    mpool->data_size = data_size;

    TAILQ_INIT(&mpool->hdr_free);
    TAILQ_INIT(&mpool->hdr_used);
    if (n*data_size > 0) {
        mpool->buffer = (char *)malloc(n*MPOOL_BLOCK_SIZE(data_size));
        if (mpool->buffer == NULL) 
            return -1;
        for (size_t i=0; i<n; i++) {
            TAILQ_INSERT_TAIL(&mpool->hdr_free, (mpool_elm_t *)(mpool->buffer + MPOOL_BLOCK_SIZE(data_size)*i), entry);
        }
    } else {
        mpool->buffer = NULL;
    }
    if (mux_init(&mpool->lock) != 0)
        return -1;

    return 0;
}

/**
 * @brief   create mpool
 * @param   n           number of data element
 *          data_size   max size of user data
 *
 * @return  pointer to the mpool created
 **/
mpool_t* mpool_new(size_t n, size_t data_size)
{
    mpool_t *mp = (mpool_t *)malloc(sizeof(mpool_t));
    int ret = mpool_init(mp, n, data_size);
    if (mp && (ret != 0)) {
        free(mp);
        mp = NULL;
    }
    return mp;
}

/**
 * @brief   destroy mpool, mpool will be inavailable after clean done,
 *          mpool must be init before using it
 * @param   mpool   mpool to be cleaned
 * @return  0 is ok
 *
 * mpool not available including lock/unlock after destroy!!!
 **/
int mpool_destroy(mpool_t *mpool)
{
    if (mpool == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (mux_lock(&mpool->lock) != 0)
        return -1;
    if (mpool->mode == MPOOL_MODE_DGROWN) {
        mpool_elm_t *p;
        TAILQ_FOREACH_REVERSE(p, &mpool->hdr_free, __mpool_head, entry) {
            TAILQ_REMOVE(&mpool->hdr_free, p, entry);
            free(p);
        }
        TAILQ_FOREACH_REVERSE(p, &mpool->hdr_used, __mpool_head, entry) {
            TAILQ_REMOVE(&mpool->hdr_used, p, entry);
            free(p);
        }
    } 

    TAILQ_INIT(&mpool->hdr_free);
    TAILQ_INIT(&mpool->hdr_used);
    if (mpool->mode == MPOOL_MODE_ISTATIC && mpool->buffer){
        free(mpool->buffer);
    }
    mpool->buffer = NULL;
    mpool->data_size = 0;
    mpool->mode = MPOOL_MODE_DESTROY;
    mux_unlock(&mpool->lock);

    mux_destroy(&mpool->lock);
    return 0;
}

/**
 * @brief   set mpool buffer, mpool mode will be set to MPOOL_MODE_ESTATIC.
 * @param   mpool       mpool to be set
 *          buf         external buffer
 *          buf_size    buffer size
 *          data_size   max size of user data
 *
 * @return  0 is ok. -1 returned if mpool is in-service.
 *
 * mpool must be empty before mpool_setbuf called, example:
 *      mpool_init(pool, 0, 0);
 *      mpool_setbuf(...);
 **/
int mpool_setbuf(mpool_t *mpool, char *buf, size_t buf_size, size_t data_size)
{
    if (mpool == NULL || buf == NULL || data_size == 0) {
        errno = EINVAL;
        return -1;
    }
    if (mux_lock(&mpool->lock) != 0)
        return -1;
    if ((!TAILQ_EMPTY(&mpool->hdr_used)) || (!TAILQ_EMPTY(&mpool->hdr_free)) || (!mpool->buffer)) {
        mux_unlock(&mpool->lock);
        errno = EBUSY;
        return -1;
    }
    mpool->buffer = buf;
    size_t n = buf_size / MPOOL_BLOCK_SIZE(data_size);
    for (size_t i=0; i<n; i++) {
        TAILQ_INSERT_TAIL(&mpool->hdr_free, (mpool_elm_t *)(buf + MPOOL_BLOCK_SIZE(data_size)*i), entry);
    }
    mpool->data_size = data_size;
    mpool->mode = MPOOL_MODE_ESTATIC;
    mux_unlock(&mpool->lock);
    return 0;
}

/**
 * @brief   alloc memory block from mpool
 * @param   mpool   mpool to be used
 *          size    size of block wanted
 *
 * @return  block's data field allocated by mpool
 *
 * block = block_head + block_data, head for mpool managerment & data for user
 **/
void* mpool_malloc(mpool_t *mpool, size_t size)
{
    if (mpool == NULL) {
        errno = EINVAL;
        return NULL;
    }

    if (mux_lock(&mpool->lock) != 0)
        return NULL;
    if (mpool->mode == MPOOL_MODE_MALLOC) {
        mux_unlock(&mpool->lock);
        return malloc(size);
    }

    if (size <= mpool->data_size) {
        if (!TAILQ_EMPTY(&mpool->hdr_free)) {
            mpool_elm_t *p = TAILQ_FIRST(&mpool->hdr_free);
            TAILQ_REMOVE(&mpool->hdr_free, p, entry);
            TAILQ_INSERT_TAIL(&mpool->hdr_used, p, entry);
            mux_unlock(&mpool->lock);
            return p->data;
        } else {
            if (mpool->mode == MPOOL_MODE_DGROWN) {
                mpool_elm_t *p = (mpool_elm_t *)malloc(MPOOL_BLOCK_SIZE(mpool->data_size));
                if (p) {
                    TAILQ_INSERT_TAIL(&mpool->hdr_used, p, entry);
                    mux_unlock(&mpool->lock);
                    return p->data;
                }
            } 
            mux_unlock(&mpool->lock);
            errno = ENOMEM;
            return NULL;
        }
    } else {
        mux_unlock(&mpool->lock);
        errno = ENOMEM;
        return NULL;
    }
}

/**
 * @brief   free memory block to mpool
 * @param   mpool   mpool to be used
 *          mem     block's data to be free
 *
 * @return  void
 *
 * block = block_head + block_data, head for mpool managerment & data for user
 **/
void mpool_free(mpool_t *mpool, void *mem)
{
    if (mpool && mem) {
        if (mux_lock(&mpool->lock) != 0)
            return;
        if (mpool->mode == MPOOL_MODE_MALLOC) {
            mux_unlock(&mpool->lock);
            free(mem);
            return;
        }
        mpool_elm_t *p = CONTAINER_OF(mem, mpool_elm_t, data);
        TAILQ_REMOVE(&mpool->hdr_used, p, entry);
        TAILQ_INSERT_TAIL(&mpool->hdr_free, p, entry);
        mux_unlock(&mpool->lock);
    }
}

#ifdef __cplusplus
}
#endif

