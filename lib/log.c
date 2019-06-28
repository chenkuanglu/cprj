/**
 * @file    log.c
 * @author  ln
 * @brief   log信息打印，允许附加打印前缀、可重定向到文件、可设定log等级
 **/

#include "log.h"

#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <libgen.h>
 
#ifdef __cplusplus
extern "C" {
#endif

static log_cb_t __stdlog = STDLOG_INITIALIZER;
log_cb_t *stdlog = &__stdlog;

/**
 * @brief   打印格式化日期：'[2019-01-01 23:59:59] '
 * @param   stream  输出流
 * @return  成功返回实际打印字符数，失败返回-1并设置errno
 **/
int log_prefix_date(FILE *stream)
{
    if (stream == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct tm ltm; 
    time_t now = time(NULL); 
    localtime_r(&now, &ltm);
    int num = fprintf(stream, "[%04d-%02d-%02d %02d:%02d:%02d] ",
                     ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
                     ltm.tm_hour, ltm.tm_min, ltm.tm_sec);                                  
    return num;
}

/**
 * @brief   初始化log对象
 *
 * @param   lcb     未初始化的log对象
 *
 * @retval  0       成功
 * @retval  -1      失败并设置errno
 */
int log_init(log_cb_t *lcb)
{
    if (lcb == NULL) {
        errno = EINVAL;
        return -1;
    }
    lcb->lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    lcb->level = LOG_PRI_DEBUG;
    lcb->stream = NULL;
    lcb->prefix_callback = log_prefix_date;
    return 0;
}

/**
 * @brief   创建log对象
 *
 * @param   lcb     log对象指针的指针
 
 * @return  返回新建的log对象，并将该log对象赋给*lcb（如果lcb不为NULL的话）
 * @retval  !NULL   成功
 * @retval  NULL    失败并设置errno
 *
 * @par 示例：
 * @code
 * log_cb_t *log = log_new(NULL);
 * @endcode
 * 或者
 * @code
 * log_cb_t *log = NULL;
 * log_new(&log);
 * @endcode
 *
 * @attention 返回的log对象需要free
 */
log_cb_t* log_new(log_cb_t **lcb)
{
    log_cb_t *p = (log_cb_t *)malloc(sizeof(log_cb_t));
    log_init(p);
    if (lcb != NULL)
        *lcb = p;
    return p;
}

/**
 * @brief   设置打印等级
 * @param   lcb     log对象
 * @param   level   打印等级
 * @return  成功返回0，失败返回-1并设置errno
 */
int log_set_level(log_cb_t *lcb, int level)
{
    pthread_mutex_lock(&lcb->lock);
    lcb->level = level;
    pthread_mutex_unlock(&lcb->lock);
    return 0;
}

/**
 * @brief   设置文件流
 * @param   lcb     log对象
 * @param   stream  文件流指针
 * @return  成功返回0，失败返回-1并设置errno
 */
int log_set_stream(log_cb_t *lcb, FILE *stream)
{
    if (lcb == NULL || stream == NULL) {
        errno = EINVAL;
        return -1;
    }
    pthread_mutex_lock(&lcb->lock);
    lcb->stream = stream;
    pthread_mutex_unlock(&lcb->lock);
    return 0;
}

/**
 * @brief   设置打印前缀的回调函数
 * @param   lcb     log对象
 * @param   prefix  打印前缀的回调函数, NULL表示不打印前缀
 * @return  成功返回0，失败返回-1并设置errno
 */
int log_set_prefix(log_cb_t *lcb, log_prefix_t prefix)
{
    if (lcb == NULL) {
        errno = EINVAL;
        return -1;
    }
    pthread_mutex_lock(&lcb->lock);
    lcb->prefix_callback = prefix;
    pthread_mutex_unlock(&lcb->lock);
    return 0;
}

/**
 * @brief   打印信息到文件
 *
 * @param   lcb         log对象
 * @param   level       log等级
 * @param   format      格式化字符串
 * @param   param       参数表
 *
 * @return  成功返回实际打印的字符数，失败返回-1并设置errno
 */
int log_vfprintf(log_cb_t *lcb, int level, const char *format, va_list param)
{
    if (lcb == NULL || format == NULL) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&lcb->lock);
    if (level < lcb->level) {
        pthread_mutex_unlock(&lcb->lock);
        return 0;
    }
    int num = 0;
    FILE *s = lcb->stream;
    if (lcb->stream == NULL)
        s = stdout;
    if (lcb->prefix_callback != NULL) 
        num += lcb->prefix_callback(s);
    num += vfprintf(s, format, param);         
    fflush(s);
    pthread_mutex_unlock(&lcb->lock);
    
    return num;
}

/**
 * @brief   打印信息到文件
 *
 * @param   lcb         log对象
 * @param   level       log等级
 * @param   format      格式化字符串
 * @param   ...         不定参数
 *
 * @return  成功返回实际打印的字符数，失败返回-1并设置errno
 */
int log_fprintf(log_cb_t *lcb, int level, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);    
    int num = log_vfprintf(lcb, level, format, arg);  
    va_end(arg); 

    return num;
}

/**
 * @brief   打印信息到屏幕
 *
 * @param   level       log等级
 * @param   format      格式化字符串
 * @param   param       参数表
 *
 * @return  成功返回实际打印的字符数，失败返回-1并设置errno
 */
int log_vprintf(int level, const char *format, va_list param)
{
    return log_vfprintf(stdlog, level, format, param);
}

/**
 * @brief   打印信息到屏幕
 *
 * @param   level       log等级
 * @param   format      格式化字符串
 * @param   ...         不定参数
 *
 * @return  成功返回实际打印的字符数，失败返回-1并设置errno
 */
int log_printf(int level, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);    
    int num = log_vprintf(level, format, arg);  
    va_end(arg); 

    return num;
}

#ifdef __cplusplus
}
#endif

