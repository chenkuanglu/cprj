/**
 * @file    cr190.h
 * @author  ln
 * @brief   hardware definition
 **/

#ifndef __CR190__
#define __CR190__

#include <stdint.h>
#include "global.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CR190_MAX_FREQ          1200        // 1200MHz

#define DADDR_08    DADDR[0]
#define DADDR_09    DADDR[1]
#define DADDR_07    DADDR[2]    // fract

typedef struct {
    double      div;
    int         len;            // 2=(08 09); 3=(08 09 07)
    uint32_t    DADDR[6];       // 08 09 07
} cr190_freq_t;

extern int cr190_reset_init     (void);
extern int cr190_hw_reset       (void);

extern int cr190_read           (int fd, uint8_t id, uint8_t reg);
extern int cr190_write          (int fd, uint8_t id, uint8_t reg, uint32_t data);
extern int cr190_write_l        (int fd, uint8_t id, uint8_t reg, char *data, int len);

extern int cr190_k7_fcalc       (uint32_t freq, uint32_t fmax, cr190_freq_t *fcb);
extern int cr190_k7_fwrite      (int fd, int id, cr190_freq_t *fcb);

extern int cr190_freq_config    (int fd, int id, uint32_t freq, cr190_freq_t *fcb);

#ifdef __cplusplus
}
#endif

#endif /* __CR190__ */

