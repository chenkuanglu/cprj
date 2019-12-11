/**
 * @file    timetick.c
 * @author  ln
 * @brief   时间操作
 */

#include "timetick.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   获取系统运行时间（不受RTC时间修改的影响）
 * @return  成功返回单调时间（单位秒），失败返回-1并设置errno
 */
double monotime(void)
{
    struct timespec tms;
    if (timespec_get(&tms, TIME_MONO) != TIME_MONO) {
        return -1;
    }
    return SPEC2DOUBLE(tms);
}

/**
 * @brief   时间等待
 * @param   sec     需要等待的时间，单位秒
 * @return  成功返回0，失败返回-1并设置errno
 */
int nsleep(double sec)
{
    struct timespec tms;
    DOUBLE2SPEC(tms, sec);
    thrd_sleep(&tms, NULL);
    return 0;
}

/**
 * @brief   time_t 转成 (GMT)struct tm
 *
 * @param   result  存储转换结果
 * @param   timep   需要被转换的time_t
 *
 * @return  等同于函数gmtime_r()
 */
struct tm* time2gmt(struct tm *result, const time_t *timep)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    return gmtime_s(result, timep);
#else
    return gmtime_r(timep, result);
#endif
}

/**
 * @brief   time_t 转成 (LOCAL)struct tm
 *
 * @param   result  存储转换结果
 * @param   timep   需要被转换的time_t
 *
 * @return  等同于函数localtime_r()
 */
struct tm* time2local(struct tm *result, const time_t *timep)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    return localtime_s(result, timep);
#else
    return localtime_r(timep, result);
#endif
}

/**
 * @brief   struct tm 转成 (LOCAL) 字符串
 *
 * @param   str     存储转换结果
 * @param   tm      需要被转换的struct tm
 * @param   size    buffer size
 *
 * @return  成功返回转换后的字符串，失败返回NULL并设置errno
 */
char* time2str(char *str, const struct tm *tm, int size)
{
    if (str == NULL || tm == NULL) {
        errno = EINVAL;
        return NULL;
    }
    snprintf(str, size, "%04d-%02d-%02d %02d:%02d:%02d",
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec);
    return str;
}

static void digit_extract(char *buf, const char *str, int size)
{
    while (*str) {
        if (isdigit(*str) && size--) {
            *buf++ = *str;
        }
        str++;
    }
}

/**
 * @brief   string 转成 (LOCAL)struct tm
 *
 * @param   result  存储转换结果
 * @param   str     需要被转换的string(一个包含数字的字符串,"*2000*01*01*23*59*59*")
 *                  例如：2000-01-01 23:59:59, 2000_0101_235959, 2000-01-01 等
 *
 * @return  成功返回转换结果，失败返回NULL并设置errno
 */
struct tm* str2local(struct tm *result, const char *str)
{
    if (result == NULL || str == NULL) {
        errno = EINVAL;
        return NULL;
    }
    
    #define size (sizeof("20001230235959"))
    char buf[size] = {0};
    memset(buf, '0', size-1);
    digit_extract(buf, str, size-1);
    memset(result, 0, sizeof(struct tm));

    int tmp = strtol(buf+8, 0, 0);
    result->tm_hour = tmp/10000;
    result->tm_min = tmp%10000/100;
    result->tm_sec = tmp%10000%100;
    buf[8] = '\0';

    tmp = strtol(buf, 0, 0);
    result->tm_year = tmp/10000 - 1900;
    result->tm_mon = tmp%10000/100 - 1;
    result->tm_mday = tmp%10000%100;

    // set wday, yday, isdst
    time_t tim = mktime(result);
    localtime_r(&tim, result);

    return result;
}

#ifdef __cplusplus
}
#endif

