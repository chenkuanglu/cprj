/**
 * @file    timetick.c
 * @author  ln
 * @brief   时间操作
 */

#include <unistd.h>
#include <time.h>
#include <errno.h>
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
    if (clock_gettime(CLOCK_MONOTONIC, &tms) != 0) {
        return -1;
    }
    return SPEC2DOUBLE(tms);
}

/**
 * @brief   时间等待
 * @param   sec     需要等待的时间，单位秒
 * @return  成功返回0，失败返回-1并设置errno
 **/
int nsleep(double sec)
{
    struct timespec tms;
    DOUBLE2SPEC(tms, sec);
    return nanosleep(&tms, NULL);
}

#ifdef __cplusplus
}
#endif
