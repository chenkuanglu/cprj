/**
 * @file    log.h
 * @author  ln
 * @brief   log信息打印，允许附加打印前缀、可重定向到文件、可设定log等级
 **/

#ifndef __LOG_INFO_H__
#define __LOG_INFO_H__

/* for using PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP, recursive and not inner process */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

#include "cstr.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Control Sequence Introducer */
#define CONSOLE_CSI_BEGIN               \x1b[
#define CONSOLE_CSI_END                 m

#define SGR_FOREGROUND                  3
#define SGR_BACKGROUND                  4
#define SGR_NOT                         2
#define SGR_GRAY_DARK                   90

#define SGR_BLACK                       0
#define SGR_RED                         1
#define SGR_GREEN                       2
#define SGR_YELLOW                      3
#define SGR_BLUE                        4
#define SGR_PURPLE                      5
#define SGR_CYAN                        6
#define SGR_WHITE                       7

#define SGR_INIT                        0
#define SGR_HIGHLIGHT                   1
#define SGR_VAGUE                       2
#define SGR_ITALIC                      3
#define SGR_UNDERLINE                   4
#define SGR_BLINK_QUIK                  5
#define SGR_BLINK_LOW                   6
#define SGR_REVERSE                     7
#define SGR_HIDE                        8
#define SGR_DELETE                      9

/* color in console: \x1b[n(;...)m */
#define CSIB                            MAKE_CSTR(CONSOLE_CSI_BEGIN)
#define CSIE                            MAKE_CSTR(CONSOLE_CSI_END)

#define CCL_BLACK                       CSIB MAKE_CSTR(CONCAT_STRING(SGR_FOREGROUND,SGR_BLACK)) CSIE
#define CCL_RED                         CSIB MAKE_CSTR(CONCAT_STRING(SGR_FOREGROUND,SGR_RED)) CSIE
#define CCL_GREEN                       CSIB MAKE_CSTR(CONCAT_STRING(SGR_FOREGROUND,SGR_GREEN)) CSIE
#define CCL_YELLOW                      CSIB MAKE_CSTR(CONCAT_STRING(SGR_FOREGROUND,SGR_YELLOW)) CSIE
#define CCL_BLUE                        CSIB MAKE_CSTR(CONCAT_STRING(SGR_FOREGROUND,SGR_BLUE)) CSIE
#define CCL_PURPLE                      CSIB MAKE_CSTR(CONCAT_STRING(SGR_FOREGROUND,SGR_PURPLE)) CSIE
#define CCL_CYAN                        CSIB MAKE_CSTR(CONCAT_STRING(SGR_FOREGROUND,SGR_CYAN)) CSIE
#define CCL_WHITE                       CSIB MAKE_CSTR(CONCAT_STRING(SGR_FOREGROUND,SGR_WHITE)) CSIE
#define CCL_WHITE_HL                    CSIB MAKE_CSTR(CONCAT_STRING(SGR_FOREGROUND,SGR_WHITE)) ";" \
                                        MAKE_CSTR(SGR_HIGHLIGHT) CSIE
#define CCL_GRAY_DARK                   CSIB MAKE_CSTR(SGR_GRAY_DARK) CSIE
#define CCL_END                         CSIB MAKE_CSTR(SGR_INIT) CSIE
#define CCL_INIT                        CCL_END

#define LOG_PRI_DEBUG                   0   ///< 调试信息，最低等级
#define LOG_PRI_INFO                    1   ///< 一般信息
#define LOG_PRI_NOTIFY                  2   ///< 重要信息
#define LOG_PRI_WARNING                 3   ///< 警告信息
#define LOG_PRI_ERROR                   4   ///< 错误信息，最高等级

typedef int (*log_prefix_t)(FILE *);        ///< 打印log前缀的回调函数

typedef struct {
    pthread_mutex_t lock;                   ///< 互斥锁
    int             level;                  ///< 当前打印等级，如果打印语句的优先级低于level，则不会打印
    FILE*           stream;                 ///< 打开(fopen)的文件流
    log_prefix_t    prefix_callback;        ///< 用来打印log前缀的回调函数
} log_cb_t;

/* print prefix without lock */
extern int log_prefix_date(FILE *stream);   ///< 用于打印日期和时间

/* stdlog initializer, NULL stream means stdout */
#define STDLOG_INITIALIZER  { PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP, 0, NULL, log_prefix_date }

extern log_cb_t *stdlog;                    ///< log模块默认创建一个标准log对象（打印到屏幕、打印前缀为当前时间、打印等级为最低）

extern int          log_init(log_cb_t *lcb);
extern log_cb_t*    log_new(log_cb_t **lcb);

extern int          log_set_level(log_cb_t *lcb, int level);
extern int          log_set_stream(log_cb_t *lcb, FILE *stream);
extern int          log_set_prefix(log_cb_t *lcb, log_prefix_t prefix);

extern int          log_vfprintf(log_cb_t *lcb, int level, const char *format, va_list param);
extern int          log_fprintf(log_cb_t *lcb, int level, const char *format, ...);

/* use stdlog(stdout + date_prefix) */
extern int          log_vprintf(int level, const char *format, va_list param);
extern int          log_printf(int level, const char *format, ...);

/// 打印调试信息到屏幕
#define logd(format, ...)       log_printf(LOG_PRI_DEBUG, CCL_GRAY_DARK format CCL_END, ##__VA_ARGS__)
/// 打印普通信息到屏幕
#define logi(format, ...)       log_printf(LOG_PRI_INFO, format, ##__VA_ARGS__)
/// 打印重要信息到屏幕
#define logn(format, ...)       log_printf(LOG_PRI_NOTIFY, CCL_WHITE_HL format CCL_END, ##__VA_ARGS__)
/// 打印警告信息到屏幕
#define logw(format, ...)       log_printf(LOG_PRI_WARNING, CCL_YELLOW format CCL_END, ##__VA_ARGS__)
/// 打印错误信息到屏幕
#define loge(format, ...)       log_printf(LOG_PRI_ERROR, CCL_RED format CCL_END, ##__VA_ARGS__)

/// 打印调试信息到任意文件
#define slogd(s, format, ...)   log_fprintf(s, LOG_PRI_DEBUG, CCL_GRAY_DARK format CCL_END, ##__VA_ARGS__)
/// 打印普通信息到任意文件
#define slogi(s, format, ...)   log_fprintf(s, LOG_PRI_INFO, format, ##__VA_ARGS__)
/// 打印重要信息到任意文件
#define slogn(s, format, ...)   log_fprintf(s, LOG_PRI_NOTIFY, CCL_WHITE_HL format CCL_END, ##__VA_ARGS__)
/// 打印警告信息到任意文件
#define slogw(s, format, ...)   log_fprintf(s, LOG_PRI_WARNING, CCL_YELLOW format CCL_END, ##__VA_ARGS__)
/// 打印错误信息到任意文件
#define sloge(s, format, ...)   log_fprintf(s, LOG_PRI_ERROR, CCL_RED format CCL_END, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif  /* __LOG_INFO_H__ */

