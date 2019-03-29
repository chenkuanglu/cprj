/**
 * @file    core.h
 * @author  ln
 * @brief   core
 **/

#ifndef __CORE_H__
#define __CORE_H__

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

// start info
typedef struct {
    pid_t       pid;
    pthread_t   tid;
    int         argc;
    char**      argv;
    double      tm;
} start_info_t;

#define __weak  __attribute__((weak))

extern double   spec2double(struct timespec *tms);
extern int      double2spec(double tm, struct timespec *tms);

extern double   monotime(void);
extern int      thrsleep(double tm);

void __weak     app_init(start_info_t *sinfo);

#ifdef __cplusplus
}
#endif 

#endif

