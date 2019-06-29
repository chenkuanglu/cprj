/**
 * @file    cstr.c
 * @author  ln
 * @brief   提供方便的字符串操作
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "cstr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   字符串转为小写
 *
 * @param   s       被转换的字符串
 *
 * @retval  !NULL   成功返回字符串s
 * @retval  NULL    失败并设置errno
 */
char* strlwr(char *s)
{
    if (s == NULL) {
        errno = EINVAL;
        return NULL;
    }
    char *p = s;
    while (*p) {
        *p = (char)tolower((int)(*p));
        p++;
    }
    return s;
}

/**
 * @brief   字符串转为大写
 *
 * @param   s       被转换的字符串
 *
 * @retval  !NULL   成功返回字符串s
 * @retval  NULL    失败并设置errno
 */
char* strupr(char *s)
{
    if (s == NULL) {
        errno = EINVAL;
        return NULL;
    }
    char *p = s;
    while (*p) {
        *p = (char)toupper((int)(*p));
        p++;
    }
    return s;
}

/**
 * @brief   删除给定字符串开始和结尾两处的所有空格
 * @param   s   被操作的字符串
 * @return  成功返回过滤后的字符串长度，失败返回-1并设置errno
 */
int strstrip(char *s)
{
    char *last = NULL ;
    char *dest = s;

    if (s == NULL) {
        errno = EINVAL;
        return -1;
    }

    last = s + strlen(s);
    while (isspace((int)*s) && *s)
        s++;
    while (last > s) {
        if (!isspace((int)*(last-1)))
            break ;
        last-- ;
    }
    *last = (char)0;

    memmove(dest, s, last-s+1);
    return last - s;
}


/**
 * @brief   二进制转换为hex
 *
 * @param   hex     输出的hex字符串
 * @param   bin     输入的二进制数据
 * @param   len     输入二进制数据的长度
 *
 * @return  成功返回hex字符串的长度(len*2)，失败返回-1并设置errno
 *
 * @attention   hex缓存的大小至少是(len*2 + 1)，否则发生溢出
 */
int bin2hex(char *hex, const void *bin, size_t len)
{
    if (bin == NULL || hex == NULL || len == 0) {
        errno = EINVAL;
        return -1;
    }
    size_t i = 0;
    for (i = 0; i < len; i++) {
        sprintf(&hex[i*2], "%02x", ((unsigned char *)bin)[i]);
    }
    hex[i*2] = '\0';
    return (int)(i*2);
}


/**
 * @brief   二进制转换为hex，输出缓存由函数负责malloc
 *
 * @param   bin     输入的二进制数据
 * @param   len     输入二进制数据的长度
 *
 * @return  成功返回hex字符串指针，失败返回NULL并设置errno
 *
 * @attention   返回的hex缓存指针，需要free
 */
char* abin2hex(const void *bin, size_t len)
{
    if (bin == NULL || len == 0) {
        errno = EINVAL;
        return NULL;
    }

    char *s = (char*)malloc((len * 2) + 1);
    if (s != NULL) {
        bin2hex(s, bin, len);
    }
    return s;
}


/**
 * @brief   hex转换为二进制
 *
 * @param   bin     输出的二进制数据
 * @param   hex     输入的hex字符串
 * @param   len     输出二进制数据的缓存长度
 *
 * @return  成功返回输出的二进制数据的长度，失败返回-1并设置errno
 *
 * @attention   hex不能以0x开头；hex的有效转换长度必须是偶数；参数len指的是输出缓存的长度而不是输入的长度
 *
 * @note        因为输入的hex字符串往往只是另一个更长串的某个部分，\n
 *              例如'1f2f3f4f, 1a2a3a4a,...'，此时函数遇到非法字符则自动停止(不会导致返回-1)，\n
 *              或者字符串并非以0结束，例如通信数据包中的字符串['a'，'b'，'c', 'd']，此时只要\n
 *              控制len(=2)便可避免越界
 */
int hex2bin(void *bin, const char *hex, size_t len)
{
    if (bin == NULL || hex == NULL || len == 0) {
        errno = EINVAL;
        return -1;
    }

    size_t len2 = len;
    char buf[4] = {0};
    while (isxdigit(*hex) && len) {
        buf[0] = *hex++;
        buf[1] = *hex++;
        *((char *)bin) = (char)strtol(buf, NULL, 16);
        len--;
        bin = (char *)bin + 1;
    }
    return (int)(len2 - len);
}

/**
 * @brief   hex转换为二进制，输出缓存由函数负责malloc
 *
 * @param   hex     输入的hex字符串
 * @param   bin_len 输出缓存里已转换的字节数，输出缓存最大长度是strlen(hex); bin_len可以为NULL
 *
 * @return  成功返回二进制数据的指针，失败返回NULL并设置errno
 *
 * @attention   hex串必须以0结束; 返回的二进制数据指针需要free
 */
void* ahex2bin(const char *hex, size_t *bin_len)
{
    if (hex == NULL) {
        errno = EINVAL;
        return NULL;
    }

    size_t len = strlen(hex);
    unsigned char *b = (unsigned char*)malloc(len/2 + 1);
    if (b != NULL) {
        size_t ret = hex2bin(b, hex, len/2 + 1);
        if (bin_len)
            *bin_len = ret;
    }
    return (void *)b;
}

/**
 * @brief   以字节为单位，反转指定长度的内存块
 *
 * @param   out             输出缓存
 * @param   in              输入缓存，in和out可以相同
 * @param   len             输入数据的大小
 * @param   section_size    需要被反转的块的大小，len必须是块大小的整数倍;\n
 *                          如果section_size为0,则section_size将被视为与len相等
 *
 * @return  成功返回0，失败返回-1并设置errno
 *
 * @par 举例
 *
 * 按一个字节反转：
 * @code
 * memswap(out,int,4,2);                        // [0][1][2][3] ===> [1][0][3][2]
 * memswap(out,int,4,4):                        // [0][1][2][3] ===> [3][2][1][0]
 * @endcode
 *
 * 通过组合调用，也可以做到按多个字节作为整体进行反转：
 * @code
 * char buf[] = {1,2,3,4};
 * memswap(buf, buf, 4, 0);
 * memswap(buf, buf, 4, 2);     // result is {3,4,1,2}
 * @endcode
 */
int memswap(void *out, const void *in, size_t len, size_t section_size)
{
    if (section_size == 0) {
        section_size = len;
    }

    if (section_size == 0 || out == NULL || in == NULL || len % section_size) {
        errno = EINVAL;
        return -1;
    }

    if (len <= 1 || section_size <= 1)  // no need to swap
        return 0;

    size_t sect_half = section_size/2;
    size_t num = len/section_size;
    for (size_t i=0; i<num; i++) {
        char *pin = (char *)in + i*section_size;
        char *pout = (char *)out + i*section_size;
        size_t ss = 0;
        while (ss < sect_half) {
            char c = pin[ss];
            pout[ss] = pin[section_size - ss - 1];
            pout[section_size - ss - 1] = c;
            ss++;
        }
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

