/**
 * @file    cstr.h
 * @author  ln
 * @brief   提供方便的字符串操作
 */

#ifndef __C_STRING_H__
#define __C_STRING_H__

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _MAKE_CSTR(s)           #s                      ///< 构造字符串，如果s是宏，则结果为宏名本身
#define MAKE_CSTR(s)            _MAKE_CSTR(s)           ///< 构造字符串，如果s是宏，则结果为宏所表示的内容

#define _CONCAT_STRING(l, r)    l##r                    ///< 连接字符串，如果l,r是宏，则结果为宏名本身
#define CONCAT_STRING(l, r)     _CONCAT_STRING(l, r)    ///< 连接字符串，如果l,r是宏，则结果为宏所表示的内容

extern char*    strlwr          (char *s);
extern char*    strupr          (char *s);

extern int      strstrip        (char *s);

extern int      bin2hex         (char *hex, const void *bin, size_t len);
extern char*    abin2hex        (const void *bin, size_t len);

extern int      hex2bin         (void *bin, const char *hex, size_t len);
extern void*    ahex2bin        (const char *hex, size_t *bin_len);

extern int      memswap         (void *out, const void *in, size_t len, size_t section_size);

#ifdef __cplusplus
}
#endif

#endif  /* __C_STRING_H__ */

