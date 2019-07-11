/**
 * @file    deveces.h
 * @author  ln
 * @brief   hardware rd/wr/ctrl
 **/

#ifndef __DEVICES__
#define __DEVICES__

#include <stdint.h>
#include "cr190.h"

#ifdef __cplusplus
extern "C" {
#endif

#define cr184_write(fd, id, reg, data)      cr190_write(fd, id, reg, data)
#define cr184_write_l(fd, id, reg, p, len)  cr190_write_l(fd, id, reg, p, len)

extern int dev_write(int fd, int id, int reg, uint32_t *pdata, int len);

#ifdef __cplusplus
}
#endif

#endif /* __DEVICES__ */

