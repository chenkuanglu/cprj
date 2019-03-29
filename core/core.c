/**
 * @file    core.c
 * @author  ln
 * @brief   core 
 **/

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif 

// timespec to double
double spec2double(struct timespec *tms)
{
    if (tms == NULL) {
        errno = EINVAL;
        return -1;
    }
    return (double)tms->tv_sec + ((double)tms->tv_nsec)/1e9;
}

// double to timespec
int double2spec(double tm, struct timespec *tms)
{
    if (tms == NULL) {
        errno = EINVAL;
        return 0;
    }
    tms->tv_sec = (time_t)tm;
    tms->tv_nsec = (long)((tm - (long)tm) * 1e9);
    return 0;
}

// get monotonic time clocks 
double monotime(void)
{
    struct timespec tms;
    if (clock_gettime(CLOCK_MONOTONIC, &tms) != 0) {
        return -1;
    }
    return (double)tms.tv_sec + ((double)tms.tv_nsec)/1e9;
}

// thread sleep 
int thrsleep(double tm)
{
    struct timespec tms;
    double2spec(tm, &tms);
    return nanosleep(&tms, NULL);
}

#ifdef __cplusplus
}
#endif 

