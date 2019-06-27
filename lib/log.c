/**
 * @file    log.c
 * @author  ln
 * @brief   print log with any prefix into file stream
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
log_cb_t * const stdlog = &__stdlog;

/**
 * @brief   print format date like '[2019-01-01 23:59:59] '
 * @param   stream  output stream
 * @return  number of char printed
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
 * @brief   init log control block
 * @param   lcb     log control block
 * @return  0 is ok
 **/
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
 * @brief   malloc & init log control block
 * @param   lcb     pointer to pointer of log control block
 * @return  the pointer to the log control block created, NULL returned if fail to new
 **/
log_cb_t* log_new(log_cb_t **lcb)
{
    log_cb_t *p = (log_cb_t *)malloc(sizeof(log_cb_t));
    log_init(p);
    if (lcb != NULL)
        *lcb = p;
    return p;
}

int log_set_level(log_cb_t *lcb, int level)
{
    pthread_mutex_lock(&lcb->lock);
    lcb->level = level;
    pthread_mutex_unlock(&lcb->lock);
    return 0;
}

/**
 * @brief   set the log output stream
 * @param   lcb     log control block
 *          stream  output file
 * @return  0 is ok
 **/
int log_set_stream(log_cb_t *lcb, FILE *stream)
{
    if (lcb == NULL || stream == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (log_lock(lcb) != 0)
        return -1;
    lcb->stream = stream;
    log_unlock(lcb);
    return 0;
}

/**
 * @brief   set the log prefix callback
 * @param   lcb     log control block
 *          prefix  prefix callback function, set NULL means prefix disabled
 * @return  0 is ok
 **/
int log_set_prefix(log_cb_t *lcb, log_prefix_t prefix)
{
    if (lcb == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (log_lock(lcb) != 0)
        return -1;
    lcb->prefix_callback = prefix;
    log_unlock(lcb);
    return 0;
}

/**
 * @brief   print format string with any prefix into file stream
 * @param   lcb         log control block
 *          format      format string
 *          param       parameter list
 *
 * @return  number of char printed
 **/
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
 * @brief   print format string with any prefix into file stream
 * @param   lcb         log control block
 *          format      format string
 *          ...         variable parameters
 *
 * @return  number of char printed
 **/
int log_fprintf(log_cb_t *lcb, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);    
    int num = log_vfprintf(lcb, format, arg);  
    va_end(arg); 

    return num;
}

/**
 * @brief   print format string with date([2018-01-01 23:59:59]) prefix
 * @param   format      format string
 *          param       parameter list
 *
 * @return  number of char printed
 **/
int log_vprintf(const char *format, va_list param)
{
    return log_vfprintf(stdlog, format, param);
}

/**
 * @brief   print format string with date([2018-01-01 23:59:59]) prefix
 * @param   format      format string
 *          ...         variable parameters
 *
 * @return  number of char printed
 **/
int log_printf(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);    
    int num = log_vprintf(format, arg);  
    va_end(arg); 

    return num;
}

#ifdef __cplusplus
}
#endif

