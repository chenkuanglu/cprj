/**
 * @file    timetick.c
 * @author  ln
 * @brief   time & ticks 
 **/

#include <unistd.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif 

/**
 * @brief   convert timespec to double
 * @param   tms     timespec to be convert
 *          tm      double allocated by caller
 *
 * @return  if success return a pointer to the parameter 'tm'.
 *          upon error, NULL is returned and errno is set
 **/
double* spec2double(const struct timespec *tms, double *tm)
{
    if (tms == NULL || tm == NULL) {
        errno = EINVAL;
        return NULL;
    }
    *tm = (double)tms->tv_sec + ((double)tms->tv_nsec)/1e9;
    return tm;
}

/**
 * @brief   convert double to timespec
 * @param   tm      double to be convert
 *          tms     timespec allocated by caller
 *
 * @return  if success return a pointer to the parameter 'tms'.
 *          upon error, NULL is returned and errno is set
 **/
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

/**
 * @brief   get monotonic time clocks
 * @param   void
 * @return  if success return double time value,
 *          upon error, -1 is returned and errno is set
 **/
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

/**
 * @brief   time sleep
 * @param   tm  double time value
 * @return  On successfully sleeping for the requested interval returns 0.  
 *          If the call is interrupted by a signal handler or encounters an error, 
 *          then it returns -1, with errno set to indicate the error.
 **/
int nsleep(double tm)
{
    struct timespec tms;
    double2spec(tm, &tms);
    return nanosleep(&tms, NULL);
}

#ifdef __cplusplus
}
#endif 

