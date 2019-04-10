/**
 * @file    cstr.c
 * @author  ln
 * @brief   the c string process
 **/

#include "cstr.h"
 
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Convert a string to lowercase
 * @param   s       String to convert
 * @return  pointer to the new string
 **/
char* strlwr(char *s)
{
    char *p = s;
    if (p) {
        while (*p) {
            *p = (char)tolower((int)(*p));
            p++;
        }
    }
    return s;
}

/**
 * @brief   Convert a string to uppercase
 * @param   s       String to convert
 * @return  pointer to the new string
 **/
char* strupr(char *s)
{
    char *p = s;
    if (p) {
        while (*p) {
            *p = (char)toupper((int)(*p));
            p++;
        }
    }
    return s;
}

/**
 * @brief   Remove blanks at the beginning and the end of a string
 * @param   s       String to parse and alter
 * @return  New size of the string
 **/
int strstrip(char *s)
{
    char *last = NULL ;
    char *dest = s;

    if (s == NULL) 
        return 0;

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
 * @brief   Get one line from string buffer
 * @param   str     String buffer, data of str will be modified!!!
 * @return  The new/next line
 **/
char *strline(char **str)
{
    if ((str == NULL) || *(*str) == '\0')
        return NULL;
    char *s = *str;
    while (*(*str) != '\0' || *(*str) != '\n') {
        (*str)++;
    }
    *(*str) = '\0';
    (*str)++;
    return s;
}

/**
 * @brief   Convert bin to hex
 * @param   hex         Output string buffer
 *          bin         bin to convert
 *          len         lenght of the bin to convert
 *
 * @return  the number of char(exclude '\0') in hex buffer
 **/
int bin2hex(char *hex, const void *bin, unsigned len)
{
    if (bin == NULL || hex == NULL || len == 0) 
        return 0;
    unsigned i = 0;
    for (i = 0; i < len; i++) {
        sprintf(&hex[i*2], "%02x", ((unsigned char *)bin)[i]);
    }
    hex[i*2] = '\0';
    return i*2;
}

/**
 * @brief   Convert bin to hex & allocate output buffer
 * @param   bin         bin to convert
 *          len         lenght of the bin to convert
 *
 * @return  pointer to the ouput char buffer
 *
 * Need to free the pointer returned from this func
 **/
char* abin2hex(const void *bin, unsigned len)
{
    if (bin == NULL || len == 0) {
        errno = EINVAL;
        return NULL;
    }

    char *s = (char*)malloc((len * 2) + 1);
    if (s != NULL) {
        bin2hex(s, bin, len);
    } else {
        errno = ENOMEM;
        return NULL;
    }
    return s;
}

/**
 * @brief   Convert hex to bin
 * @param   bin         Output bin buffer
 *          hex         hex to convert
 *          len         lenght of output bin buffer
 *
 * @return  Number of byte in bin buffer
 *
 * the string 'fde' is same as '0fde'
 **/
int hex2bin(void *bin, const char *hex, unsigned len)
{
    if (bin == NULL || hex == NULL || len == 0) 
        return 0;

    unsigned len2 = len;
    char buf[4] = {0};
    if (strlen(hex) % 2) {
        buf[0] = *hex++;
        *((unsigned char *)bin) = (unsigned char)strtol(buf, NULL, 16);
        len--;
        bin = (unsigned char *)bin + 1;
    }
    while (*hex && len) {
        buf[0] = *hex++;
        buf[1] = *hex++;
        *((unsigned char *)bin) = (unsigned char)strtol(buf, NULL, 16);
        len--;
        bin = (unsigned char *)bin + 1;
    }
    return len2 - len;
}

/**
 * @brief   Convert hex to bin
 * @param   hex         hex to convert
 *          len         lenght of the hex to convert
 *
 * @return  pointer to the ouput bin buffer
 *
 * the string 'fde' is same as '0fde'
 * Need to free the pointer returned from this func
 **/
void * ahex2bin(const char *hex)
{
    if (hex == NULL) {
        errno = EINVAL;
        return NULL;
    }

    unsigned len = strlen(hex);
    unsigned char *b = (unsigned char*)malloc(len/2 + 1);
    if (b != NULL) {
        hex2bin(b, hex, len/2 + 1);
    } else {
        errno = ENOMEM;
        return NULL;
    }
    return (void *)b;
}

/**
 * @brief   reverse/swap specified length memory byte-to-byte
 * @param   out             output buffer
 *          in              input buffer
 *          len             size of input buffer
 *          section_size    size of memory section to swap
 *
 * len = 4:
 * section_size=2:[0][1][2][3] ===> [1][0][3][2]
 * section_size=4:[0][1][2][3] ===> [3][2][1][0], the same as big/small endian convert on integer
 *
 * section_size = 0 means section_size is the len
 *
 * @return  0 is ok 
 **/
int memswap(void *out, const void *in, unsigned len, unsigned section_size)
{
    if (out == NULL || in == NULL || len % section_size)
        return -1;
    if (len <= 1 || section_size <= 1)
        return 0;

    if (section_size == 0) {
        section_size = len;
    }
    unsigned num = len/section_size;
    for (unsigned i=0; i<num; i++) {
        char *pin = (char *)in + i*section_size;
        char *pout = (char *)out + i*section_size;
        unsigned ss = 0;
        while (ss < section_size/2) {
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

