/**
 * @file    timer.c
 * @author  ln
 * @brief   timer
 **/

#include "timer.h"
#include "../lib/timetick.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct {
    int id;
    int type;
    uint32_t ticks;
    uint32_t period;
    tmr_event_proc_t proc;
    void *arg;
} tmr_event_t;

tmr_cb_t stdtmr;
static pthread_t tid_stdtmr;

int tmr_init(tmr_cb_t *tmr, double precise)
{
    if (tmr == NULL || precise <= 0)
        return -1;
    if (que_init(&tmr->que) != 0) 
        return -1;
    tmr->precise = precise;
    return 0;
}

void* thread_stdtmr(void *arg)
{
    pthread_detach(pthread_self());
    for (;;) {
        nsleep(tmr_def.precise);
        tmr_heartbeat(&tmr_def);
    }
}

int tmr_add(tmr_cb_t *tmr, int id, int type, double time, tmr_event_proc_t proc, void *arg)
{
    if (tmr == NULL || time <= 0 || proc == NULL)
        return -1;
    tmr_event_t evt;
    evt.id = id;
    evt.type = type;
    evt.period = TMR_TIME2TICK(time, tmr->precise);
    evt.ticks = evt.period;
    evt.proc = proc;
    evt.arg = arg;
    int res = que_insert_tail(&tmr->que, &evt, sizeof(evt));
    return res;
}

int tmr_remove(tmr_cb_t *tmr, int id)
{
    que_elm_t *var;
    que_cb_t *pq = &tmr->que;
    tmr_event_t *pe;

    QUE_LOCK(pq);
    QUE_FOREACH_REVERSE(var, pq) {
        pe = (tmr_event_t *)var->data;
        if (pe->id == id) {
            QUE_REMOVE(&tmr->que, var);
            break;
        }
    }
    QUE_UNLOCK(pq);
    return 0;
}

void tmr_heartbeat(tmr_cb_t *tmr)
{
    if (tmr == NULL)
        return;

    que_elm_t *var;
    que_cb_t *pq = &tmr->que;
    tmr_event_t *pe;

    QUE_LOCK(pq);
    QUE_FOREACH_REVERSE(var, pq) {
        pe = (tmr_event_t *)var->data;
        if (pe->ticks > 0) {
            pe->ticks -= 1;
            if (pe->ticks == 0) {
                pe->proc(pe->arg);
                if (pe->type == TMR_EVENT_TYPE_PERIODIC) {
                    pe->ticks = pe->period;
                }
            }
        } else {
            QUE_REMOVE(&tmr->que, var);
        }
    }    
    QUE_UNLOCK(pq);
}

void tmr_destroy(tmr_cb_t *tmr)
{
    que_destroy(&tmr->que);
}

#ifdef __cplusplus
}
#endif 

