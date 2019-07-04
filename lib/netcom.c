/**
 * @file    netcom.c
 * @author  ln
 * @brief   udp/tcp网络通信
 **/

#include "netcom.h"
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   设置结构体sockaddr_in的ip和port
 *
 * @param   saddr  socket地址
 * @param   ip     ip地址
 * @param   port   端口号
 *
 * @return  成功返回0,失败返回-1并设置errno
 */
int net_setsaddr(struct sockaddr_in *saddr, uint32_t ip, uint16_t port)
{
    if (saddr == NULL) {
        errno = EINVAL;
        return -1;
    }
    bzero(saddr, sizeof(struct sockaddr_in));
    saddr->sin_family = AF_INET;
    saddr->sin_addr.s_addr = htonl(ip);
    saddr->sin_port = htons(port);
    return 0;
}

/**
 * @brief   udp绑定ip和端口号
 *
 * @param   local_ip    要绑定/监听的ip地址    
 * @param   local_port  要绑定/监听端口号
 *
 * @return  成功返回0,失败返回-1并设置errno
 */
int udp_bind(int fd_socket, uint32_t local_ip, uint16_t local_port)
{
    struct sockaddr_in addr_listen;
    const socklen_t slen = sizeof(struct sockaddr_in);
    net_setsaddr(&addr_listen, local_ip, local_port);
	if (bind(fd_socket, (struct sockaddr *)&addr_listen, slen) == -1) {
        close(fd_socket);
        return -1;
    }
    return fd_socket;
}

/**
 * @brief   打开socket udp
 * @return  成功返回打开的fd,失败返回-1并设置errno
 */
int udp_open(void)
{
    int opt_en = 1;
    int fd_socket = -1;

    if ((fd_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        return -1;
    }
    if (setsockopt(fd_socket, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, 
                                    (char *)&opt_en, sizeof(opt_en)) != 0) {
        close(fd_socket);
        return -1;
    }
    return fd_socket;
}

/**
 * @brief   udp接收
 *
 * @param   fd          socket  
 * @param   buf         缓存
 * @param   len         缓存大小
 * @param   flags       标志位，例如不阻塞 MSG_DONTWAIT（和O_NONBLOCK类似）
 * @param   src_addr    收到的数据包的源地址
 *
 * @return  成功返回接收个数,失败返回-1并设置errno
 */
int udp_read(int fd, void *buf, size_t len, int flags, struct sockaddr_in *src_addr)
{
    int ret = -1;
    socklen_t slen = sizeof(struct sockaddr_in);
    if ((ret = recvfrom(fd, buf, len, flags, (struct sockaddr *)src_addr, &slen)) < 0) {
        return -1;
    }
    return ret;
}

/**
 * @brief   udp发送
 *
 * @param   fd          socket  
 * @param   buf         缓存
 * @param   len         缓存大小
 * @param   flags       标志位，例如不阻塞 MSG_DONTWAIT（和O_NONBLOCK类似）
 * @param   src_addr    收到的数据包的源地址
 *
 * @return  成功返回接收个数,失败返回-1并设置errno
 */
int udp_write(int fd, const void *buf, size_t len, int flags, const struct sockaddr_in *dst_addr)
{
    socklen_t addrlen = sizeof(struct sockaddr_in);
    return sendto(fd, buf, len, flags, (const struct sockaddr *)dst_addr, addrlen);
}

#ifdef __cplusplus
}
#endif

