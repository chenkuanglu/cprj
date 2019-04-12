#ifndef __TRIGG_UTIL_H__
#define __TRIGG_UTIL_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "../core/core.h"
#include "trigg_block.h"
#include "trigg_rand.h"

#ifdef __cplusplus
extern "C" {
#endif 

#define FAR

extern uint16_t get16(void *buff);
extern void put16(void *buff, uint16_t val);

extern uint32_t get32(void *buff);
extern void put32(void *buff, uint32_t val);

extern void put64(void *buff, void *val);
extern int cmp64(void *a, void *b);
extern char *bnum2hex(uint8_t *bnum);

extern int nonblock(SOCKET sd);
extern char *ntoa(uint8_t *a);
extern uint32_t str2ip(char *addrstr);
extern void trigg_coreip_shuffle(uint32_t *list, uint32_t len);

#ifdef __cplusplus
}
#endif 

#endif

