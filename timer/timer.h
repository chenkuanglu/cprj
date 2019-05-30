/**
 * @file    timer.h
 * @author  ln
 * @brief   software timer
 **/

#ifndef __TIMERX_H__
#define __TIMERX_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif 

#include "../lib/err.h"
#include "../lib/que.h"

typedef void (*tmr_event_proc_t)(void *arg);

typedef struct {
    int id;
    int type;
    uint32_t ticks;
    uint32_t period;
    tmr_event_proc_t proc;
    void *arg;
} tmr_event_t;

typedef struct {
    que_cb_t que;
    double precise;
} tmr_cb_t;

extern tmr_cb_t tmr_def;

#define TMR_EVENT_TYPE_PERIODIC     0
#define TMR_EVENT_TYPE_ONESHOT      1

#define TMR_TIME2TICK(tm, d)        ( ceil((tm)/(d)) )

#define TMR_INIT()                  tmr_init(&stdtmr, 0.1)
#define TMR_START()                 pthread_create(&tid_stdtmr, 0, thread_stdtmr, 0);
#define TMR_ADD(id, t, d, f, arg)   tmr_add(&stdtmr, id, t, d, f, arg)

extern int tmr_init(tmr_cb_t *tmr, double precise);
extern int tmr_add(tmr_cb_t *tmr, int id, int type, double time, tmr_event_proc_t proc, void *arg);
extern int tmr_remove(tmr_cb_t *tmr, int id);
extern void tmr_heartbeat(tmr_cb_t *tmr);
extern void tmr_destroy(tmr_cb_t *tmr);

extern void* thread_stdtmr(void *arg);

#ifdef __cplusplus
}
#endif 

#endif

