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

// error number: 0 ~ 1023
#define LIB_ERRNO_BASE              256
#define LIB_ERRNO_MAX_NUM           (1024 - LIB_ERRNO_BASE)

#define LIB_ERRNO_QUE_EMPTY         (LIB_ERRNO_BASE + 0)
#define LIB_ERRNO_QUE_FULL          (LIB_ERRNO_BASE + 1)
#define LIB_ERRNO_MEM_ALLOC         (LIB_ERRNO_BASE + 2)
#define LIB_ERRNO_MBLK_SHORT        (LIB_ERRNO_BASE + 3)
#define LIB_ERRNO_RES_LIMIT         (LIB_ERRNO_BASE + 4)
#define LIB_ERRNO_SHORT_MPOOL       (LIB_ERRNO_BASE + 5)
#define LIB_ERRNO_NOT_EXIST         (LIB_ERRNO_BASE + 6)

extern int err_init(void);
extern int err_add(int errnum, const char *str);
extern char *err_string(int errnum, char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif

