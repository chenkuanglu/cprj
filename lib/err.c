/**
 * @file    err.c
 * @author  ln
 * @brief   convert error number to error string
 **/

#include "err.h"

#ifdef __cplusplus
extern "C" {
#endif 

const char unknown_str[] = "Unknown error";
static char* errtbl[LIB_ERRNO_MAX_NUM] = {0};

int err_init(void)
{
    err_add(LIB_ERRNO_QUE_EMPTY, "Queue empty");
    err_add(LIB_ERRNO_QUE_FULL, "Queue full");
    err_add(LIB_ERRNO_MEM_ALLOC, "Malloc fail");
    err_add(LIB_ERRNO_MEM_OUT, "Memory out");
    return 0;
}

// add new item {errnum,str} to list
int err_add(int errnum, const char *str)
{
    if (errnum < LIB_ERRNO_BASE || 
        errnum > (LIB_ERRNO_MAX_NUM + LIB_ERRNO_BASE - 1) || str == NULL) {
        errno = EINVAL;
        return -1;
    }
    int ix = errnum - LIB_ERRNO_BASE;
    if (errtbl[ix] != NULL) 
        free(errtbl[ix]);
    if ((errtbl[ix] = strdup(str)) == NULL)
        return -1;
    return 0;
}

// thread safe, GNU-specific
char* err_string(int errnum, char *buf, size_t size)
{
    if (errnum < 0 || buf == NULL || size < 1) {
        errno = EINVAL;
        return NULL;
    }
    if (errnum < LIB_ERRNO_BASE) {
        strerror_r(errnum, buf, size);
    } else {
        int ix = errnum - LIB_ERRNO_BASE;
        if (errtbl[ix] != NULL) {
            strncpy(buf, errtbl[ix], size);
        } else {
            snprintf(buf, size, "%s: %d", unknown_str, errnum);
        }
        buf[size-1] = '\0';
    }
    return buf;
}

#ifdef __cplusplus
}
#endif 

