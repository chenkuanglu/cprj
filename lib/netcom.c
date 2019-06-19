/**
 * @file    netcom.c
 * @author  ln
 * @brief   internet communication by udp/tcp
 **/

#include "netcom.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   fill sockaddr_in by ip & port
 *
 * @param   saddr  struct sockaddr_in
 * @param   ip     ip
 * @param   port   port
 *
 * @return  0 is ok.
 **/
int set_sockaddr(struct sockaddr_in *saddr, uint32_t ip, uint16_t port)
{
    bzero(saddr, sizeof(struct sockaddr_in));
    saddr->sin_family = AF_INET;
    saddr->sin_addr.s_addr = htonl(ip);
    saddr->sin_port = htons(port);
    return 0;
}

int udp_server_open(uint32_t local_ip, uint16_t local_port)
{
    int opt_en = 1;
    int fd_socket = -1;
    struct sockaddr_in addr_listen;
    const socklen_t slen = sizeof(struct sockaddr_in);

    if ((fd_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        return -1;
    }
    if (setsockopt(fd_socket, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, (char *)&opt_en, sizeof(opt_en)) < 0) {
        close(fd_socket);
        return -1;
    }
    set_sockaddr(&addr_listen, local_ip, local_port);
	if (bind(fd_socket, (struct sockaddr *)&addr_listen, slen) == -1) {
        close(fd_socket);
        return -1;
    }
    return fd_socket;
}

int udp_client_open(void)
{
    int opt_en = 1;
    int fd_socket = -1;

    if ((fd_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        return -1;
    }
    if (setsockopt(fd_socket, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, (char *)&opt_en, sizeof(opt_en)) < 0) {
        close(fd_socket);
        return -1;
    }
    return fd_socket;
}

int udp_read(int fd, void *buf, int len, int flags, struct sockaddr_in *src_addr)
{
    int ret = -1;
    socklen_t slen = sizeof(struct sockaddr_in);
    if ((ret = recvfrom(fd, buf, len, flags, (struct sockaddr *)src_addr, &slen)) < 0) {
        return -1;
    }
    return ret;
}

int udp_write(int fd, const void *buf, size_t len, int flags, const struct sockaddr_in *dst_addr)
{
    socklen_t addrlen = sizeof(struct sockaddr_in);
    return sendto(fd, buf, len, flags, (const struct sockaddr *)dst_addr, addrlen);
}

#ifdef __cplusplus
}
#endif

