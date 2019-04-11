#ifndef __TRIGG_UTIL_H__
#define __TRIGG_UTIL_H__

#include <stdint.h>

extern uint16_t get16(void *buff);
extern uint16_t put16(void *buff, uint16_t val);

extern uint32_t get32(void *buff);
extern uint32_t put32(void *buff, uint32_t val);

extern void put64(void *buff, void *val);
extern int cmp64(void *a, void *b);

#endif

