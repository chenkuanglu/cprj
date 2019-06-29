/**
 * @file    mpool.h
 * @author  ln
 * @brief   内存池管理，提供固定大小内存块的分配与释放(按int对齐)，避免频繁地向系统申请内存\n
 *
 *          内存池空间可以来自外部已分配好的较大buffer，也可以由模块内部自动malloc一块较大内存；\n
 *          内存池大小可以由外部指定，也可以任由模块自动增长（即当内存池为空或耗尽时，
 *          模块自动向系统一次性申请包含N个块的较大内存池）
 *          内存池每个小块的大小（即申请与释放的固定块大小）必须外部指定，否则将与malloc等效
 *
 *          内存池从系统malloc到的内存块不会被free，直到内存池管理对象被销毁(mpool_destroy)
 *
 *          使用时只需要先初始化mpool_init，之后便可以进行“分配”mpool_malloc与“释放”mpool_free
 */

#include "mpool.h"
#include "err.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OFFSET_OF(TYPE, MEMBER)             ((size_t)&((TYPE *)0)->MEMBER)
#define CONTAINER_OF(ptr, type, member)     ((type *)((char *)(ptr) - OFFSET_OF(type,member)))

/**
 * @brief   初始化内存池
 *
 * @param   mpool       内存池对象
 *          data_size   用户数据大小，如果使用外部存储ebuf，data_size最好是sizeof(int)的倍数
 *          n           最大使用的块数
 *          ebuf        外部buffer
 *
 * @return  成功返回0，失败返回-1并设置errno
 *
 * @par 模式说明
 *
 * MPOOL_MODE_DESTROYED: 未初始化
 *
 * MPOOL_MODE_MALLOC: data_size == 0
 * 系统模式：直接调用系统malloc和free
 *
 * MPOOL_MODE_DGROWN: n == 0, data_size != 0
 * 自增长模式：内存块不够时自动malloc一个较大内存块并分割为小块以供分配
 *
 * MPOOL_MODE_ISTATIC: n != 0, data_size != 0, ebuf == NULL
 * 内部模式：mpool只在初始化时malloc一个指定大小的内存块，模块保证能够分配n个块
 *
 * MPOOL_MODE_ESTATIC: n != 0, data_size != 0, ebuf != NULL
 * 外部模式：由外部参数传入一个内存块，内存块大小被认定为 n*data_size
 *
 * @attention   由于数据大小 data_size 总是被强制与sizeof(int)对齐，\n
 *              所以在外部模式下，实际可用的块数目 m 可能少于请求的数目 n，\n
 *              例如 mpool_init(mpl,3,1,ebuf)会返回-1，因为 (1*3)/4=0，即无块可用
 */
int mpool_init(mpool_t *mpool, size_t data_size, size_t n, void *ebuf)
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
        size_t ebuf_size = data_size * n;

        // sizeof(int) align
        int align = data_size % sizeof(int);
        if (align)
            data_size += sizeof(int) - align;

        if (n == 0) {
            mpool->mode = MPOOL_MODE_DGROWN;
        } else {
            if (ebuf) {
                mpool->mode = MPOOL_MODE_ESTATIC;
                mpool->sbuf = ebuf;
                n = ebuf_size / data_size;
                if (n == 0) {
                    errno = EINVAL;
                    return -1;
                }
            } else {
                mpool->mode = MPOOL_MODE_ISTATIC;
            }
        }
    }
    mpool->data_size = data_size;

    TAILQ_INIT(&mpool->hdr_free);
    TAILQ_INIT(&mpool->hdr_used);
    TAILQ_INIT(&mpool->hdr_buf);
    char *p;
    if (ebuf == NULL && n*data_size > 0) {      // MPOOL_MODE_ISTATIC
        if ((p = (char *)malloc(MPOOL_BLOCK_SIZE(n*MPOOL_BLOCK_SIZE(data_size)))) == NULL)
            return -1;
        else
            TAILQ_INSERT_HEAD(&mpool->hdr_buf, (mpool_elm_t *)p, entry);
    }

    char *buf_data;
    if (ebuf)
        buf_data = ebuf;
    else
        buf_data = ((mpool_elm_t *)p)->data;

    for (size_t i=0; i<n; i++) {
        TAILQ_INSERT_HEAD(  &mpool->hdr_free,
                            (mpool_elm_t *)(buf_data + MPOOL_BLOCK_SIZE(data_size)*i),
                            entry   );
    }
    return 0;
}

/**
 * @brief   新建内存池对象
 * @param   n           最大使用的块数
 *          data_size   用户数据大小，如果使用外部存储ebuf，data_size最好是sizeof(int)的倍数
 *          ebuf        外部buffer
 *
 * @return      成功返回内存池对象指针，失败返回NULL
 * @attention   返回的对象需要free
 */
mpool_t* mpool_new(size_t data_size, size_t n, void *ebuf)
{
    mpool_t *mp = (mpool_t *)malloc(sizeof(mpool_t));
    if (mp == NULL)
        return NULL;

    int ret = mpool_init(mp, data_size, n, ebuf);
    if (mp && (ret != 0)) {
        free(mp);
        mp = NULL;
    }
    return mp;
}

/**
 * @brief   销毁内存池对象
 * @param   mpool   内存池对象指针
 * @return  成功返回0，失败返回-1并设置errno
 */
int mpool_destroy(mpool_t *mpool)
{
    if (mpool == NULL) {
        errno = EINVAL;
        return -1;
    }
    mux_lock(&mpool->lock);
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
 * @brief   从内存池中分配数据
 * @param   mpool   内存池对象指针
 *          size    想要分配的数据大小，如果size大于块的有效数据大小，则总是分配失败
 *
 * @return  成功返回块内有效数据的指针，失败返回NULL并设置errno
 *
 * @note    大块 = 大块的表头 + (N*块)，块 = 块的表头 + 有效数据区
 **/
void* mpool_malloc(mpool_t *mpool, size_t size)
{
    if (mpool == NULL) {
        errno = EINVAL;
        return NULL;
    }

    mux_lock(&mpool->lock);
    if (mpool->mode == MPOOL_MODE_MALLOC) {
        mux_unlock(&mpool->lock);
        return malloc(size);
    }

    if (size <= mpool->data_size) {
        if (!TAILQ_EMPTY(&mpool->hdr_free)) {
            mpool_elm_t *p = TAILQ_LAST(&mpool->hdr_free, __mpool_head);
            TAILQ_REMOVE(&mpool->hdr_free, p, entry);
            TAILQ_INSERT_HEAD(&mpool->hdr_used, p, entry);
            mux_unlock(&mpool->lock);
            return p->data;
        } else {
            if (mpool->mode == MPOOL_MODE_DGROWN) {
                size_t grown_size = MPOOL_BLOCK_SIZE(MPOOL_BLOCK_NUM_ALLOC *
                                                        MPOOL_BLOCK_SIZE(mpool->data_size));
                char *pbuf = (char *)malloc(grown_size);
                if (pbuf) {
                    TAILQ_INSERT_HEAD(&mpool->hdr_buf, (mpool_elm_t *)pbuf, entry);
                    pbuf = ((mpool_elm_t *)pbuf)->data;
                    size_t i;
                    for (i=1; i<MPOOL_BLOCK_NUM_ALLOC; i++) {
                        TAILQ_INSERT_HEAD(  &mpool->hdr_free,
                                            (mpool_elm_t *)(pbuf+(i*MPOOL_BLOCK_SIZE(mpool->data_size))),
                                            entry   );
                    }
                    TAILQ_INSERT_HEAD(&mpool->hdr_used, (mpool_elm_t *)pbuf, entry);
                    mux_unlock(&mpool->lock);
                    return ((mpool_elm_t *)pbuf)->data;
                }
            }
            mux_unlock(&mpool->lock);
            errno = LIB_ERRNO_SHORT_MPOOL;
            return NULL;
        }
    } else {
        mux_unlock(&mpool->lock);
        errno = LIB_ERRNO_MBLK_SHORT;
        return NULL;
    }
}

/**
 * @brief   向内存池中释放数据
 * @param   mpool   内存池对象指针
 *          mem     要释放的有效数据指针，该指针指向某个块的有效数据区
 *
 * @return  void
 *
 * @note    程序将mem指向的地址减去一个偏移就得到了块指针
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

