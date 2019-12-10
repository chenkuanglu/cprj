/**
 * @file    timetick.h
 * @author  ln
 * @brief   时间操作
 */

#ifndef __TIMETICK_H__
#define __TIMETICK_H__

#include "threads_c11.h"
#include <string.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

/// timespec 转成 double
#define SPEC2DOUBLE(s)      ( (double)((s).tv_sec) + ((double)((s).tv_nsec))/1e9 )
/// double 转成 timespec
#define DOUBLE2SPEC(ts, d)  \
    do { \
        (ts).tv_sec = (time_t)(d); \
        (ts).tv_nsec = (long)(((d) - (long)(d)) * 1e9); \
    } while (0)

extern double   monotime(void);
extern int      nsleep(double tm);

extern struct tm*   time2gmt(struct tm *result, const time_t *timep);
extern struct tm*   time2local(struct tm *result, const time_t *timep);
extern char*        time2str(char *str, const struct tm *tm, int size);

extern struct tm*   str2local(struct tm *result, const char *str);

#ifdef __cplusplus
}
#endif

#endif

