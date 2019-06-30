/**
 * @file    timer.h
 * @author  ln
 * @brief   软定时器\n
 *          模块已经定义了一个标准定时器以供使用，通过 TMR_START() 可以开启；
 *          通过 tmr_add() 可以注册一个定时事件，tmr_remove()可以删除事件；
 *
 *          定时器每次检查完事件后，通过等待指定的时间间隔来完成计数，所以是不精确定时
 *
 *          如果需要创建多种精度的定时器，只需要重新初始化一个定时器对象，并创建一个线程：
 * @code
 * tmr_cb_t timer;
 * tmr_init(&timer, 60.0);  // 1min
 *
 * void* thread_timer(void *arg)
 * {
 *     pthread_detach(pthread_self());
 *     for (;;) {
 *         nsleep(timer.precise);
 *         tmr_heartbeat(&timer);
 *     }
 * }
 *
 * pthread_create(&timer, 0, thread_timer, 0);
 * @endcode
 */

#include "timer.h"
#include "../lib/timetick.h"
#include "../lib/err.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int id;                 ///< 事件ID
    int type;               ///< 事件类型，周期或者单次
    unsigned ticks;         ///< 离超时还有多少ticks
    unsigned period;        ///< 定时时间
    tmr_event_proc_t proc;  ///< 超时回调函数
    void *arg;              ///< 回调参数，该参数在事件注册时被指定
} tmr_event_t;

tmr_cb_t stdtmr;
pthread_t tid_stdtmr;
mpool_t stdmp;

/**
 * @brief   初始化定时器对象
 * @param   tmr     定时器对象
 * @param   precise 定时器精度，单位秒
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int tmr_init(tmr_cb_t *tmr, double precise)
{
    if (tmr == NULL || precise <= 0) {
        errno = EINVAL;
        return -1;
    }

    if (MPOOL_INIT_GROWN(&stdmp, QUE_BLOCK_SIZE(sizeof(tmr_event_t))) != 0)
        return -1;
    if (QUE_INIT_MP(&tmr->que, &stdmp) != 0)
        return -1;

    tmr->precise = precise;
    return 0;
}

/// 标准定时器的线程
void* thread_stdtmr(void *arg)
{
    pthread_detach(pthread_self());
    for (;;) {
        nsleep(stdtmr.precise);
        tmr_heartbeat(&stdtmr);
    }
}

/**
 * @brief   向定时器注册一个定时事件
 *
 * @param   tmr     定时器对象
 * @param   id      事件ID
 * @param   type    事件类型，TMR_EVENT_TYPE_PERIODIC或者TMR_EVENT_TYPE_ONESHOT
 * @param   time    定时时间，单位秒
 * @param   proc    回调函数
 * @param   arg     回调参数
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int tmr_add(tmr_cb_t *tmr, int id, int type, double time, tmr_event_proc_t proc, void *arg)
{
    if (tmr == NULL || time <= 0 || proc == NULL) {
        errno = EINVAL;
        return -1;
    }

    tmr_event_t evt;
    evt.id      = id;
    evt.type    = type;
    evt.period  = TMR_TIME2TICK(time, tmr->precise);
    evt.ticks   = evt.period;
    evt.proc    = proc;
    evt.arg     = arg;

    return que_insert_tail(&tmr->que, &evt, sizeof(evt));
}

/**
 * @brief   从定时器删除一个定时事件
 *
 * @param   tmr     定时器对象
 * @param   id      事件ID
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int tmr_remove(tmr_cb_t *tmr, int id)
{
    if (tmr == NULL) {
        errno = EINVAL;
        return -1;
    }

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

/**
 * @brief   定时器的tick心跳函数，用来检查所有定时事件是否超时，并决定是否调用回调函数
 * @param   tmr     定时器对象
 * @return  void
 */
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

/**
 * @brief   销毁定时器
 * @param   tmr     定时器对象
 * @return  void
 */
void tmr_destroy(tmr_cb_t *tmr)
{
    que_destroy(&tmr->que);
}

#ifdef __cplusplus
}
#endif
