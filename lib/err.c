/**
 * @file    err.c
 * @author  ln
 * @brief   将系统的、库的、app 的错误码统一起来
 *
 *          模块初始化(err_init())完成后，需要先调用err_add()增加自定义的错误码和对应字符串，
 *          之后就可以通过err_string()输出错误码所对应的字符串。
 *
 *          函数err_string()是线程安全的，和它用法类似的标准库函数是strerror_r()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cstr.h"
#include "err.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 如果错误码未定义，则err_string()输出该字符串
const char unknown_str[] = "Unknown error";

/// 字符串指针数组，代表所有错误码所对应的字符串
static char* errtbl[LIB_ERRNO_MAX_NUM] = {0};

/**
 * @brief   err模块初始化
 * @retval  0   成功
 * @retval  -1  失败并设置errno
 */
int err_init(void)
{
    int ret = 0;
    ret += err_add(LIB_ERRNO_QUE_EMPTY, "Queue empty");
    ret += err_add(LIB_ERRNO_QUE_FULL, "Queue full");
    ret += err_add(LIB_ERRNO_MEM_ALLOC, "Malloc fail");
    ret += err_add(LIB_ERRNO_MBLK_SHORT, "Block size of memory-pool not enough");
    ret += err_add(LIB_ERRNO_BUF_SHORT, "Local buffer not enough");
    ret += err_add(LIB_ERRNO_RES_LIMIT, "Resources exceed limit");
    ret += err_add(LIB_ERRNO_SHORT_MPOOL, "Short of memory-pool");
    ret += err_add(LIB_ERRNO_NOT_EXIST, "Objectives not exist");
    if (ret != 0)
        return -1;
    else
        return 0;
}

/**
 * @brief   注册一个错误码和它所对应的字符串
 *
 * @param   errnum  错误码
 * @param   str     错误码所对应的字符串，函数内部会使用strdup()对该字符串进行拷贝
 *
 * @retval  0   成功
 * @retval  -1  失败并设置errno
 */
int err_add(int errnum, const char *str)
{
    char *p;
    if (errnum < LIB_ERRNO_BASE ||
        errnum > (LIB_ERRNO_MAX_NUM + LIB_ERRNO_BASE - 1) || str == NULL) {
        errno = EINVAL;
        return -1;
    }
    if ((p = strdup(str)) == NULL)
        return -1;
    int ix = errnum - LIB_ERRNO_BASE;
    if (errtbl[ix] != NULL)
        free(errtbl[ix]);
    errtbl[ix] = p;
    return 0;
}

/**
 * @brief   输出给定错误码所对应的字符串，线程安全，用法类似标准库函数strerror_r()
 *
 * @param   errnum  错误码
 * @param   buf     字符串的输出buf，函数会确保输出的字符串含有结束符('\0')；
 *                  所以如果buf的size不足，输出的字符串会被截断。
 * @param   size    输出buf的大小
 *
 * @retval  0   成功
 * @retval  -1  失败并设置errno
 */
char* err_string(int errnum, char *buf, size_t size)
{
    if (errnum < 0 || buf == NULL || size < 1) {
        errno = EINVAL;
        return NULL;
    }
    if (errnum < LIB_ERRNO_BASE) {
#if defined(_WIN32) && !defined(__CYGWIN__)
        strerror_s(buf, size, errnum);
#else
        strerror_r(errnum, buf, size);
#endif
    } else {
        int ix = errnum - LIB_ERRNO_BASE;
        if (errtbl[ix] != NULL) {
            strncpy(buf, errtbl[ix], size);
        } else {
            snprintf(buf, size, "%s: %d", unknown_str, errnum);
        }       
    }
    buf[size-1] = '\0';
    return buf;
}

#ifdef __cplusplus
}
#endif
