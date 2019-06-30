/**
 * @file    common.h
 * @author  ln
 * @brief   进程信号同步处理、进程安全退出、基本模块初始化
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include "../lib/timetick.h"
#include "../lib/err.h"
#include "../lib/log.h"
#include "../lib/thrq.h"
#include "../lib/que.h"
#include "../lib/netcom.h"
#include "../timer/timer.h"
#include "../argparser/argparser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COMMON_RETIRE() \
    do { \
        for (;;) nsleep(60); \
    } while (0)

extern int  common_init(void);
extern void common_stop(void);

extern int  common_wait_exit(void);
extern void common_exit(int ec);

#ifdef __cplusplus
}
#endif

#endif
