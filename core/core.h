/**
 * @file    core.h
 * @author  ln
 * @brief   core
 **/

#ifndef __CORE_H__
#define __CORE_H__

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "../lib/timetick.h"
#include "../lib/err.h"
#include "../lib/log.h"

#ifdef __cplusplus
extern "C" {
#endif 

// start info
typedef struct {
    int         argc;
    char**      argv;

    pid_t       pid;        // pid of process
    pthread_t   tid;        // thread id of main thread
    double      tm;         // time of app startup
} start_info_t;

extern int          core_init(void);
extern log_cb_t*    core_getlog(void);

#ifdef __cplusplus
}
#endif 

#endif

