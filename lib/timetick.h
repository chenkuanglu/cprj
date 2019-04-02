/**
 * @file    time_tick.h
 * @author  ln
 * @brief   time & ticks 
 **/

#ifndef __TIME_TICK_H__
#define __TIME_TICK_H__

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif 

extern double   spec2double(struct timespec *tms);
extern int      double2spec(double tm, struct timespec *tms);

extern double   monotime(void);
extern int      nsleep(double tm);

#ifdef __cplusplus
}
#endif 

#endif

