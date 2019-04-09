/**
 * @file    mux.c
 * @author  ln
 * @brief   mutex, inner process & recursive
 **/

#include "mux.h"
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   init mutex, inner process & recursive
 * @param   mutex to be init
 * @return  return 0 on success. otherwise, -1 is returned on error and errno is set.
 **/
int mux_init(mux_t *mux)
{
    if (mux == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    pthread_mutexattr_init(&mux->attr);
    if ((errno = pthread_mutexattr_setpshared(&mux->attr, PTHREAD_PROCESS_PRIVATE)) != 0) 
        return -1;
    if ((errno = pthread_mutexattr_setprotocol(&mux->attr, PTHREAD_PRIO_INHERIT)) != 0) 
        return -1;
    if ((errno = pthread_mutexattr_settype(&mux->attr, PTHREAD_MUTEX_RECURSIVE)) != 0) 
        return -1;

    pthread_mutex_init(&mux->mux, &mux->attr);
    return 0;
}

/**
 * @brief   malloc & init mutex, inner process & recursive
 * @param   mux     pointer to your mutex pointer
 * @return  return a pointer to the mutex created.
 *          upon error, NULL is returned and errno is set
 **/
mux_t* mux_new(mux_t **mux)
{
    mux_t *p = (mux_t *)malloc(sizeof(mux_t));
    if (p && (mux_init(p) < 0)) {
        free(p);
        p = NULL;
    }

    if (mux != NULL)
        *mux = p;
    return p;
}

/**
 * @brief   destroy mutex 
 * @param   mux     mutex to be clean 
 * @return  void
 *
 * do not call this function if a mutex was initialized by INITIALIZER macro! 
 **/
void mux_destroy(mux_t *mux)
{
    if (mux) {
        pthread_mutexattr_destroy(&mux->attr);
        pthread_mutex_destroy(&mux->mux);
    }
}

/**
 * @brief   lock
 * @param   mutex to be lock
 * @return  return 0 on success. otherwise, -1 is returned on error and errno is set.
 **/
int mux_lock(mux_t *mux)
{
    if ((errno = pthread_mutex_lock(&mux->mux)) != 0)
        return -1;
    return 0;
}

/**
 * @brief   unlock
 * @param   mutex to be unlock
 * @return  return 0 on success. otherwise, -1 is returned on error and errno is set.
 **/
int mux_unlock(mux_t *mux)
{
    if ((errno = pthread_mutex_unlock(&mux->mux)) != 0)
        return -1;
    return 0;
}

#ifdef __cplusplus
}
#endif

