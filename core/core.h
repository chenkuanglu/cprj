/**
 * @file    core.h
 * @author  ln
 * @brief   core
 **/

#ifndef __CORE_H__
#define __CORE_H__

#include <stdint.h>
#include <stdbool.h>
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
#include "../lib/que.h"
#include "../timer/timer.h"

#ifdef __cplusplus
extern "C" {
#endif 

#define CORE_MSG_TYPE_BASE      ( -1 )
#define CORE_MSG_CMD_BASE       ( -256 )

#define CORE_MSG_TYPE_TIMER     ( CORE_MSG_TYPE_BASE - 1 )

#define CORE_MSG_CMD_EXPIRE     ( CORE_MSG_CMD_BASE - 1 )

typedef struct {
    long    type;
    long    cmd;
    double  tm;
    long    len;
    char    data[];
} core_msg_t;

typedef void* (*core_thread_t)(void *);

#define CORE_THR_RETIRE()       do {\
                                    for (;;) nsleep(60);\
                                } while (0)

extern log_cb_t *CLOG;

extern int          core_start(int argc, char **argv);
extern void         core_loop(void);
extern void         core_proper_exit(int ec);

extern int          core_add_guard(thrq_cb_t *thrq);

#ifdef __cplusplus
}
#endif 

#endif

