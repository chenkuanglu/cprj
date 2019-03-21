/**
 * @file    app_init.h
 * @author  ln
 * @brief   application init
 **/

#ifndef __APP_INIT__
#define __APP_INIT__

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    pid_t       pid;
    pthread_t   tid;
    int         argc;
    char**      argv;
} start_info_t;

extern void app_init(start_info_t *info);

#ifdef __cplusplus
}
#endif

#endif

