/**
 * @file    core.h
 * @author  ln
 * @brief   core
 **/

#ifndef __CORE_H__
#define __CORE_H__

#include <stdint.h>
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
#include "../lib/thrq.h"

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

    thrq_cb_t   tq;
} start_info_t;

typedef void* (*core_thread_t)(void *);

#define CORE_THR_RETIRE()       do {\
                                    for (;;) nsleep(60);\
                                } while (0)

extern log_cb_t *core_log;

#define CLOG        core_log 

extern int          core_init(void);
extern void         core_proper_exit(int ec);
extern void         process_proper_exit(int ec);

#ifdef __cplusplus
}
#endif 

#endif

