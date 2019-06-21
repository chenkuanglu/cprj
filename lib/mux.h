/**
 * @file    mux.h
 * @author  ln
 * @brief   创建并使用一个线程之间的、优先级继承的、可嵌套的互斥锁
 */

#ifndef __THR_MUX__
#define __THR_MUX__

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 互斥锁类型定义
 */
typedef struct {
    pthread_mutex_t     mux;        ///< linux互斥锁
    pthread_mutexattr_t attr;       ///< linux互斥锁属性
} mux_t;

extern int      mux_init    (mux_t *mux);
extern mux_t*   mux_new     (mux_t **mux);
extern void     mux_destroy (mux_t *mux);

extern int      mux_lock    (mux_t *mux);
extern int      mux_unlock  (mux_t *mux);

#ifdef __cplusplus
}
#endif

#endif // __THR_MUX__

