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
int socket_set_addr(struct sockaddr_in *saddr, uint32_t ip, uint16_t port)
{
    bzero(saddr, sizeof(struct sockaddr_in));
    saddr->sin_family = AF_INET;
    saddr->sin_addr.s_addr = htonl(ip);
    saddr->sin_port = htons(port);
    return 0;
}

int socket_netcom_init(netcom_t *netcom)
{
    if (netcom == NULL) {
        errno = EINVAL;
        return -1;
    }
    netcom->type = NETCOM_TYPE_UDP;
    netcom->longpoll = false;
    netcom->fd_server = -1;
    return mux_init(&netcom->lock);
}

// return -1 on error
int socket_server_recv(netcom_t *netcom, void *buf, int len)
{
    int ret;
    int opt_en = 1;
    int fd_socket = -1;
    const socklen_t slen = sizeof(struct sockaddr_in);
    socklen_t recv_slen = sizeof(struct sockaddr_in);
    if ((fd_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        return -1;
    }
    netcom->fd_server = fd_socket;
    if (setsockopt(fd_socket, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, (char *)&opt_en, sizeof(opt_en)) < 0) {
        close(fd_socket);
        netcom->fd_server = -1;
        return -1;
    }
	struct sockaddr_in addr_src;
	if (bind(fd_socket, (struct sockaddr *)&(netcom->addr_listen), slen) == -1) {
        close(fd_socket);
        netcom->fd_server = -1;
        return -1;
    }
	
    for (;;) {
        if ((ret = recvfrom(fd_socket, buf, len, 0, (struct sockaddr *)&(addr_src), &recv_slen)) < 0) {
            close(fd_socket);
            netcom->fd_server = -1;
            return -1;
        }
        netcom->recv_proc(fd_socket, &addr_src, buf, ret);
    }
}

#ifdef __cplusplus
}
#endif

