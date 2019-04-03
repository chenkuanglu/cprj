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

#include "../lib/timetick.h"
#include "../lib/err.h"

#ifdef __cplusplus
extern "C" {
#endif 

// start info
typedef struct {
    int         argc;
    char**      argv;
    pid_t       pid;
    pthread_t   tid;
    double      tm;
} start_info_t;

#ifdef __cplusplus
}
#endif 

#endif

