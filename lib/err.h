/**
 * @file    err.h
 * @author  ln
 * @brief   error number
 **/

#ifndef __ERR_H__
#define __ERR_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LIB_ERRNO_BASE          256
#define LIB_ERRNO_MAX_NUM       1024

extern int err_add(int errnum, const char *str);
extern char *err_string(int errnum, char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif

