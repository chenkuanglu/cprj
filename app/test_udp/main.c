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

int online_proc(int fd, struct sockaddr_in *src_addr, void *data, int len)
{
    if (len > 0) {
        logw("data from %s:%d: %s\n", inet_ntoa(src_addr->sin_addr), htons(src_addr->sin_port), (char*)data);
        sendto(fd, "ok", 2, 0, (struct sockaddr *)src_addr, sizeof(struct sockaddr_in));
    }
    return 0;
}

void* thread_online_listen(void *arg)
{
    char buf[2048] = {0};
    netcom_t netcom;
    socket_netcom_init(&netcom);
    socket_set_addr(&netcom.addr_listen, INADDR_ANY, 5510);
    netcom.recv_proc = online_proc;

    for (;;) {
        if (socket_server_recv(&netcom, buf, sizeof(buf)) < 0) {
            loge("socket recv fail: %s", strerror(errno));
            nsleep(0.1);
        }
    }
}

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

    pthread_t pid;
    pthread_create(&pid, 0, thread_online_listen, 0);

    // client
    sleep(1);
    int opt_en = 1;
    struct sockaddr_in addr;
    struct sockaddr_in client_addr;
    socklen_t len = (socklen_t)sizeof(struct sockaddr_in);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    socket_set_addr(&addr, INADDR_BROADCAST, 5510);
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, (char *)&opt_en, sizeof(opt_en));
    sendto(fd, "send myself...", 14, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    getsockname(fd, (struct sockaddr *)&client_addr, &len);
    logw("get client: %s:%d\n", inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));

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

