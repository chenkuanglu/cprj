/**
 * @file    err.h
 * @author  ln
 * @brief   将系统的、库的、app 的错误码统一起来
 *
 *          模块初始化(err_init())完成后，需要先调用err_add()增加自定义的错误码和对应字符串，
 *          之后就可以通过err_string()输出错误码所对应的字符串。
 *
 *          函数err_string()是线程安全的，和它用法类似的标准库函数是strerror_r()
 */

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

#define LIB_ERRNO_BASE              256                         ///< 系统错误码: 0 ~ 255, 函数库错误码: 256 ~ 511
/**
 * app错误码: 512 ~ 1023 
 * @par     举例：
 * @code
 * #define APP_ERRNO_XXX    (LIB_ERRNO_END + [0~511])
 * @endcode
 */
#define LIB_ERRNO_END               512                    
#define LIB_ERRNO_MAX_NUM           (1024 - LIB_ERRNO_BASE)     ///< 除去系统错误码，剩下(1024-256)个由函数库负责管理

#define LIB_ERRNO_QUE_EMPTY         (LIB_ERRNO_BASE + 0)        ///< 队列空
#define LIB_ERRNO_QUE_FULL          (LIB_ERRNO_BASE + 1)        ///< 队列满
#define LIB_ERRNO_MEM_ALLOC         (LIB_ERRNO_BASE + 2)        ///< malloc分配失败
#define LIB_ERRNO_MBLK_SHORT        (LIB_ERRNO_BASE + 3)        ///< 内存池中的固定块大小低于想要分配的数据块
#define LIB_ERRNO_BUF_SHORT         (LIB_ERRNO_BASE + 4)        ///< 缓存不足
#define LIB_ERRNO_RES_LIMIT         (LIB_ERRNO_BASE + 5)        ///< 资源使用到达设定的上限
#define LIB_ERRNO_SHORT_MPOOL       (LIB_ERRNO_BASE + 6)        ///< 内存池数据块耗尽
#define LIB_ERRNO_NOT_EXIST         (LIB_ERRNO_BASE + 7)        ///< 目标不存在

extern int      err_init(void);
extern int      err_add(int errnum, const char *str);
extern char*    err_string(int errnum, char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif

