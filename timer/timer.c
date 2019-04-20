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

tmr_cb_t tmr_def;
static pthread_t tid_tmr_def;

int tmr_init(tmr_cb_t *tmr)
{
    if (tmr == NULL) 
        return -1;
    if (que_init(&tmr->que) != 0) 
        return -1;
    return 0;
}

// 100ms 
static void* tmr_thread_def(void *arg)
{
    pthread_t tid = pthread_self();
    pthread_detach(tid);
    for (;;) {
        nsleep(0.1);
        tmr_heartbeat(&tmr_def);
    }
}

int tmr_init_def(void)
{
    tmr_init(&tmr_def);
    pthread_create(&tid_tmr_def, 0, tmr_thread_def, 0);
    return 0;
}

int tmr_add(tmr_cb_t *tmr, int id, int type, int period, tmr_event_proc_t proc, void *arg)
{
    if (tmr == NULL)
        return -1;
    tmr_event_t evt;
    evt.id = id;
    evt.type = type;
    evt.period = period;
    evt.ticks = period;
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
    QUE_FOREACH(var, pq) {
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
    QUE_FOREACH(var, pq) {
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

