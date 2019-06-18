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

#define NETCOM_TYPE_UDP     1
#define NETCOM_TYPE_TCP     2

typedef int (*socket_recv_callback_t)(int fd, struct sockaddr_in *src_addr, void *data, int len);

typedef struct {
    mux_t lock;
    int type;       // udp/tcp
    bool longpoll;  // tcp only

    int fd_server;
	struct sockaddr_in addr_listen;
    socket_recv_callback_t recv_proc;
} netcom_t;

extern int socket_netcom_init(netcom_t *netcom);
extern int socket_set_addr(struct sockaddr_in *saddr, uint32_t ip, uint16_t port);
extern int socket_server_recv(netcom_t *netcom, void *buf, int len);

#ifdef __cplusplus
}
#endif

#endif

