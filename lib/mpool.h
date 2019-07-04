/**
 * @file    mpool.h
 * @author  ln
 * @brief   内存池管理，提供固定大小内存块的分配与释放(按int对齐)，避免频繁地向系统申请内存\n
 *
 *          内存池空间可以来自外部已分配好的较大buffer，也可以由模块内部自动malloc一块较大内存；\n
 *          内存池大小可以由外部指定，也可以任由模块自动增长（即当内存池为空或耗尽时，
 *          模块自动向系统一次性申请包含N个块的较大内存块）
 *          内存池每个小块的大小（即申请与释放的固定块大小）必须外部指定，否则将与malloc等效
 *
 *          内存池从系统malloc到的内存块不会被free，直到内存池管理对象被销毁(mpool_destroy)
 *
 *          使用时只需要先初始化mpool_init，之后便可以进行“分配”mpool_malloc与“释放”mpool_free
 */

#ifndef __MEMORY_POOL__
#define __MEMORY_POOL__

#include <sys/queue.h>
#include "mux.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MPOOL_MODE_DESTROYED = 0,   ///< 无效：对象未初始化或者已经销毁
    MPOOL_MODE_MALLOC,          ///< 系统模式：直接调用系统malloc和free
    MPOOL_MODE_DGROWN,          ///< 自增长模式：内存块不够时自动malloc一个较大内存块并分割为小块
    MPOOL_MODE_ISTATIC,         ///< 内部模式：mpool只在初始化时malloc一个指定大小的内存块
    MPOOL_MODE_ESTATIC          ///< 外部模式：由外部参数传入一个内存块
};

typedef struct __mpool_elm {
    TAILQ_ENTRY(__mpool_elm)    entry;      ///< 块的表头，块与块基于此构成一张链表
    char                        data[];     ///< 块的有效数据
} mpool_elm_t;

typedef TAILQ_HEAD(__mpool_head, __mpool_elm) mpool_head_t;

typedef struct __mpool {
    mpool_head_t    hdr_free;   ///< 总空闲块（是一张链表）
    mpool_head_t    hdr_used;   ///< 总已用块（是一张链表）
    mux_t           lock;       ///< 互斥锁
    size_t          data_size;  ///< 块内有效数据的大小（不是块的总大小）
    mpool_head_t    hdr_buf;    ///< 所有malloc到的大块（是一张链表）
    char*           sbuf;       ///< 指向外部给定的buffer
    int             mode;       ///< 内存池工作模式
} mpool_t;

/// 内存池在自增长时一次性malloc的总块数
#define MPOOL_BLOCK_NUM_ALLOC           256
/// 块大小（块的表头 + 块的有效数据）
#define MPOOL_BLOCK_SIZE(data_size)     (sizeof(mpool_elm_t) + data_size)
/// 长度按int对齐
#define MPOOL_ALIGN_SIZE(len)           ((len) ? ((((len)-1) / sizeof(int)) + 1) * sizeof(int) : 0)

#define MPOOL_INIT_MALLOC(mpl)          mpool_init(mpl,0,0,0)
#define MPOOL_INIT(mpl)                 MPOOL_INIT_MALLOC(mpl)

#define MPOOL_INIT_GROWN(mpl,s)         mpool_init(mpl,s,0,0)

#define MPOOL_INIT_ISTATIC(mpl,s,n)     mpool_init(mpl,s,n,0)

#define MPOOL_INIT_ESTATIC(mpl,s,n,e)   mpool_init(mpl,s,n,e)

extern int          mpool_init(mpool_t *mpool, size_t data_size, size_t n, void *ebuf);
extern mpool_t*     mpool_new(size_t data_size, size_t n, void *ebuf);
extern int          mpool_destroy(mpool_t *mpool);

extern void*        mpool_malloc(mpool_t *mpool, size_t size);
extern void         mpool_free(mpool_t *mpool, void *mem);

#ifdef __cplusplus
}
#endif

#endif
