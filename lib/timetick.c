/**
 * @file    time_tick.c
 * @author  ln
 * @brief   time & ticks 
 **/

#include <unistd.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif 

// timespec to double
double* spec2double(const struct timespec *tms, double *tm)
{
    if (tms == NULL || tm == NULL) {
        errno = EINVAL;
        return NULL;
    }
    *tm = (double)tms->tv_sec + ((double)tms->tv_nsec)/1e9;
    return tm;
}

// double to timespec
struct timespec* double2spec(double tm, struct timespec *tms)
{
    if (tms == NULL) {
        errno = EINVAL;
        return NULL;
    }
    tms->tv_sec = (time_t)tm;
    tms->tv_nsec = (long)((tm - (long)tm) * 1e9);
    return tms;
}

// get monotonic time clocks 
double monotime(void)
{
    double tm;
    struct timespec tms;
    if (clock_gettime(CLOCK_MONOTONIC, &tms) != 0) {
        return -1;
    }
    if (spec2double(&tms, &tm) == NULL) {
        return -1;
    }
    return tm;
}

// thread sleep 
int nsleep(double tm)
{
    struct timespec tms;
    double2spec(tm, &tms);
    return nanosleep(&tms, NULL);
}

#ifdef __cplusplus
}
#endif 

