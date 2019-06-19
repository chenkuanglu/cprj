/**
 * @file    netcom.h
 * @author  ln
 * @brief   internet communication by udp/tcp
 **/

#ifndef __NETCOM_H__
#define __NETCOM_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "mux.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GET_SOCKADDR(s)     ( (s).sin_addr )
#define GET_SOCKIP(s)       ( htonl((s).sin_addr.s_addr) )
#define GET_SOCKPORT(s)     ( htons((s).sin_port) )

extern int set_sockaddr(struct sockaddr_in *saddr, uint32_t ip, uint16_t port);

extern int udp_server_open(uint32_t local_ip, uint16_t local_port);
extern int udp_client_open(void);

extern int udp_read(int fd, void *buf, int len, int flags, struct sockaddr_in *src_addr);
extern int udp_write(int fd, const void *buf, size_t len, int flags, const struct sockaddr_in *dst_addr);

#ifdef __cplusplus
}
#endif

#endif

