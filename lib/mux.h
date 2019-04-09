/**
 * @file    mux.h
 * @author  ln
 * @brief   mutex, inner process & recursive
 **/

#ifndef __THR_MUX__
#define __THR_MUX__

#include <pthread.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    pthread_mutex_t     mux;
    pthread_mutexattr_t attr;
} mux_t;

extern int      mux_init    (mux_t *mux);
extern mux_t*   mux_new     (mux_t **mux);
extern void     mux_destroy (mux_t *mux);

extern int      mux_lock    (mux_t *mux);
extern int      mux_unlock  (mux_t *mux);

#ifdef __cplusplus
}
#endif

#endif /* __THR_MUX__ */

