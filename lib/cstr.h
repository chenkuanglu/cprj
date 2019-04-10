/**
 * @file    cstr.h
 * @author  ln
 * @brief   the c string process
 **/

#ifndef __C_STRING_H__
#define __C_STRING_H__

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* make string */
#define _MAKE_CSTR(s)           #s
#define MAKE_CSTR(s)            _MAKE_CSTR(s)

/* concat string */
#define _CONCAT_STRING(l, r)    l##r
#define CONCAT_STRING(l, r)     _CONCAT_STRING(l, r)

extern char*    strlwr          (char *s);
extern char*    strupr          (char *s);

extern int      strstrip        (char *s);
extern char*    strline         (char **str);

extern int      bin2hex         (char *hex, const void *bin, unsigned len);
extern char*    abin2hex        (const void *bin, unsigned len);

extern int      hex2bin         (void *bin, const char *hex, unsigned len);
extern void*    ahex2bin        (const char *hex);

extern int      memswap         (void *out, const void *in, unsigned len, unsigned section_size);

#ifdef __cplusplus
}
#endif

#endif  /* __C_STRING_H__ */

