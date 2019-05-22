/**
 * @file    mpool.c
 * @author  ln
 * @brief   memory pool managerment
 **/

#include "mpool.h"
#include "err.h"

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
 * MPOOL_MODE_DESTROYED: mpool been cleaned
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

    if (mux_init(&mpool->lock) != 0)
        return -1;
    mpool->sbuf = NULL;

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
    TAILQ_INIT(&mpool->hdr_buf);
    if (n*data_size > 0) {  // MPOOL_MODE_ISTATIC
        char *p = (char *)malloc(MPOOL_BLOCK_SIZE(n*MPOOL_BLOCK_SIZE(data_size)));
        if (p == NULL) 
            return -1;
        else 
            TAILQ_INSERT_HEAD(&mpool->hdr_buf, (mpool_elm_t *)p, entry);
        char *buf_data = ((mpool_elm_t *)p)->data;
        for (size_t i=0; i<n; i++) {
            TAILQ_INSERT_HEAD(&mpool->hdr_free, (mpool_elm_t *)(buf_data + MPOOL_BLOCK_SIZE(data_size)*i), entry);
        }
    }//

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
    if (mp == NULL)
        return NULL;

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
    if (mpool->mode == MPOOL_MODE_DGROWN || mpool->mode == MPOOL_MODE_ISTATIC) {
        mpool_elm_t *p;
        TAILQ_FOREACH_REVERSE(p, &mpool->hdr_buf, __mpool_head, entry) {
            TAILQ_REMOVE(&mpool->hdr_buf, p, entry);
            free(p);
        }
    } 

    TAILQ_INIT(&mpool->hdr_free);
    TAILQ_INIT(&mpool->hdr_used);
    mpool->sbuf = NULL;
    mpool->data_size = 0;
    mpool->mode = MPOOL_MODE_DESTROYED;
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
 *      or
 *      MPOOL_INIT_ESTATIC(...);
 **/
int mpool_setbuf(mpool_t *mpool, char *buf, size_t buf_size, size_t data_size)
{
    if (mpool == NULL || buf == NULL || data_size == 0) {
        errno = EINVAL;
        return -1;
    }
    if (mux_lock(&mpool->lock) != 0)
        return -1;
    if (!TAILQ_EMPTY(&mpool->hdr_used)) {
        mux_unlock(&mpool->lock);
        errno = EBUSY;
        return -1;
    }

    if (mpool->mode == MPOOL_MODE_DGROWN || mpool->mode == MPOOL_MODE_ISTATIC) {
        mpool_elm_t *p;
        TAILQ_FOREACH_REVERSE(p, &mpool->hdr_buf, __mpool_head, entry) {
            TAILQ_REMOVE(&mpool->hdr_buf, p, entry);
            free(p);
        }
    } 
    TAILQ_INIT(&mpool->hdr_free);

    mpool->sbuf = buf;
    size_t n = buf_size / MPOOL_BLOCK_SIZE(data_size);
    for (size_t i=0; i<n; i++) {
        TAILQ_INSERT_HEAD(&mpool->hdr_free, (mpool_elm_t *)(buf + MPOOL_BLOCK_SIZE(data_size)*i), entry);
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
            mpool_elm_t *p = TAILQ_LAST(&mpool->hdr_free, __mpool_head);
            TAILQ_REMOVE(&mpool->hdr_free, p, entry);
            TAILQ_INSERT_TAIL(&mpool->hdr_used, p, entry);
            mux_unlock(&mpool->lock);
            return p->data;
        } else {
            if (mpool->mode == MPOOL_MODE_DGROWN) {
                char *pbuf = (char *)malloc(MPOOL_BLOCK_SIZE(MPOOL_BLOCK_NUM_ALLOC*MPOOL_BLOCK_SIZE(mpool->data_size)));
                if (pbuf) {
                    TAILQ_INSERT_HEAD(&mpool->hdr_buf, (mpool_elm_t *)pbuf, entry);
                    pbuf = ((mpool_elm_t *)pbuf)->data;
                    size_t i;
                    for (i=1; i<MPOOL_BLOCK_NUM_ALLOC; i++) {
                        TAILQ_INSERT_HEAD(&mpool->hdr_free, (mpool_elm_t *)(pbuf+(i*MPOOL_BLOCK_SIZE(mpool->data_size))), entry);
                    }
                    TAILQ_INSERT_TAIL(&mpool->hdr_used, (mpool_elm_t *)pbuf, entry);
                    mux_unlock(&mpool->lock);
                    return ((mpool_elm_t *)pbuf)->data;
                }
            } 
            mux_unlock(&mpool->lock);
            return NULL;
        }
    } else {
        mux_unlock(&mpool->lock);
        errno = LIB_ERRNO_MBLK_SHORT;
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
        TAILQ_INSERT_HEAD(&mpool->hdr_free, p, entry);
        mux_unlock(&mpool->lock);
    }
}

#ifdef __cplusplus
}
#endif

