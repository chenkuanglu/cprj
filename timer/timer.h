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

#ifndef __SOFT_TIMER_H__
#define __SOFT_TIMER_H__

#include <math.h>
#include "../lib/que.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 定时器超时后的回调函数，注意回调函数应当尽快处理事件并退出，否则会耽误其他事件的处理
typedef void (*tmr_event_proc_t)(void *arg);

typedef struct {
    que_cb_t    que;        ///< 存储所有定时请求
    double      precise;    ///< 定时器的精度（每隔precise秒检查一次）
} tmr_cb_t;

extern tmr_cb_t stdtmr;     ///< 预定义的标准定时器，精度0.1s
extern pthread_t tid_stdtmr;

#define TMR_EVENT_TYPE_PERIODIC     0   ///< 周期性触发回调函数
#define TMR_EVENT_TYPE_ONESHOT      1   ///< 只触发一次回调函数

/// 定时时间 / 精度 = ticks (向上去整: ceil(19.4) = 20)
#define TMR_TIME2TICK(tm, d)        ( ceil((tm)/(d)) )

/// 启动标准定时器（精度0.1s）
#define TMR_START()                 \
    do { \
        tmr_init(&stdtmr, 0.1); \
        pthread_create(&tid_stdtmr, 0, thread_stdtmr, 0); \
    } while (0)
/// 停止标准定时器
#define TMR_STOP()                  tmr_destroy(&stdtmr);

/// 向标准定时器注册一个定时事件，参数分别是：时间ID、类型、定时时间、回调函数和参数
#define TMR_ADD(id, t, d, fn, arg)  tmr_add(&stdtmr, id, t, d, fn, arg)
/// 从标准定时器取消一个定时事件，需要指定事件ID
#define TMR_REMOVE(id)              tmr_remove(&stdtmr, id)

extern int  tmr_init(tmr_cb_t *tmr, double precise);
extern int  tmr_add(tmr_cb_t *tmr, int id, int type, double time, tmr_event_proc_t proc, void *arg);
extern int  tmr_remove(tmr_cb_t *tmr, int id);
extern void tmr_heartbeat(tmr_cb_t *tmr);
extern void tmr_destroy(tmr_cb_t *tmr);

extern void* thread_stdtmr(void *arg);

#ifdef __cplusplus
}
#endif

#endif
