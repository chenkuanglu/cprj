/**
 * @file    main.c
 * @author  ln
 * @brief   main function
 *
 * run: build/app/testlib/testlib --aaa 1 --bbb 1 0x12 3
 **/

#include "../../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif 

argparser_t *cmdline;
int cmdline_proc(long id, char **param, int num);

int main(int argc, char **argv)
{
    if (core_init(argc, argv) != 0) {
        loge("core_init() fail: %d\n", errno);
        return 0;
    }

    cmdline = argparser_new(argc, argv);
    argparser_add(cmdline, "--aaa", 'a', 1);
    argparser_add(cmdline, "--bbb", 'b', 2);
    argparser_parse(cmdline, cmdline_proc);

    // que add
    int n;
    que_cb_t *que = que_new(NULL);
    n = 1;
    que_insert_head(que, &n, sizeof(n));
    n = 10;
    que_insert_head(que, &n, sizeof(n));
    n = 100;
    que_insert_head(que, &n, sizeof(n));
    n = 1000;
    que_insert_head(que, &n, sizeof(n));
    n = 10000;
    que_insert_head(que, &n, sizeof(n));

    // que find/remove
    n = 100;
    que_remove(que, &n, sizeof(n), 0);

    // que foreach
    que_elm_t *var;
    QUE_LOCK(que);
    QUE_FOREACH_REVERSE(var, que) {
        int *pint = QUE_ELM_DATA(var, int);
        logi("que elm: %d\n", *pint);
    }
    QUE_UNLOCK(que);

    core_wait_exit();
}

int cmdline_proc(long id, char **param, int num)
{
    switch (id) {
        case 'a':
            logi("'id=%c',n=%d,(%d)\n", id, num, strtol(param[0], 0, 0));
            break;
        case 'b':
            logi("'id=%c',n=%d,(%d %d)\n", id, num, strtol(param[0], 0, 0), strtol(param[1], 0, 0));
            break;
        default:
            break;
    }
    return 0;
}

void app_proper_exit(int ec)
{
    core_stop();
    argparser_delete(cmdline);
}

#ifdef __cplusplus
}
#endif 



//////////////////////////////////////////////////////

typedef int (*socket_recv_callback_t)(struct sockaddr_in *src_addr, void *data, int len);
typedef struct {
    mux_t lock;
	struct sockaddr_in addr_listen;
    socket_recv_callback_t recv_proc;
} netcom_t;

int online_proc(struct sockaddr_in *src_addr, void *data, int len)
{
    if (len > 0) {
        logw("data from %s:%d: %s", inet_ntoa(src_addr->sin_addr), src_addr->sin_port, (char*)data);
    }
    return 0;
}

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
    return mux_init(&netcom->lock);
}

int socket_recv(netcom_t *netcom, void *buf, int len)
{
    int opt_en = 1;
    int fd_socket = -1;
    socklen_t slen = sizeof(struct sockaddr_in);
    if ((fd_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        return -1;
    }
    if (setsockopt(fd_socket, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, (char *)&opt_en, sizeof(opt_en)) < 0) {
        close(fd_socket);
        return -1;
    }
	struct sockaddr_in addr_src;
	if (bind(fd_socket, (struct sockaddr *)&(netcom->addr_listen), slen) == -1) {
        close(fd_socket);
        return -1;
    }
	
    for (;;) {
        int ret = recvfrom(fd_socket, buf, len, 0, (struct sockaddr *)&(addr_src), &slen);
        if (ret < 0) {
            close(fd_socket);
            return -1;
        }
        netcom->recv_proc(&addr_src, buf, ret);
    }
}


// 等待PC管理工具发送心跳包
void *thread_online_listen(void *unused)
{
    char buf[2048] = {0};
    netcom_t netcom;
    socket_netcom_init(&netcom);
    socket_set_addr(&netcom.addr_listen, INADDR_ANY, PORT_ONLINE_NOTIFY);
    netcom.recv_proc = online_proc;

    for (;;) {
        if (socket_recv(&netcom, buf, sizeof(buf)) < 0) {
            loge("socket recv fail: %s", strerror(errno));
            usleep(100*1000);
        }
    }
}

//////////////////////////////////////////////////////


