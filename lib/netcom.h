/**
 * @file    netcom.h
 * @author  ln
 * @brief   udp/tcp网络通信
 */

#ifndef __NETCOM_H__
#define __NETCOM_H__

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NET_SOCKADDR(s)     ( (s).sin_addr )
#define NET_SOCKIP(s)       ( htonl((s).sin_addr.s_addr) )
#define NET_SOCKPORT(s)     ( htons((s).sin_port) )

extern int net_setsaddr(struct sockaddr_in *saddr, uint32_t ip, uint16_t port);

extern int udp_open(void);
extern int udp_bind(int fd_socket, uint32_t local_ip, uint16_t local_port);

extern int udp_read(int fd, void *buf, size_t len, int flags, struct sockaddr_in *src_addr);
extern int udp_write(int fd, const void *buf, size_t len, int flags, const struct sockaddr_in *dst_addr);

#ifdef __cplusplus
}
#endif

#endif

