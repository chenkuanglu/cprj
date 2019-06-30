/**
 * @file    main.c
 * @author  ln
 * @brief   main function
 *
 * run: build/app/testlib/testlib --aaa 1 --bbb 1 0x12 3
 **/

#include "../../common/common.h"

#ifdef __cplusplus
extern "C" {
#endif

argparser_t *cmdline;
int cmdline_proc(long id, char **param, int num);

void* thread_udp_server(void *arg)
{
    char buf[100] = {0};
    const char *sinfo = "server info";
    struct sockaddr_in src_addr;

    int fd = udp_server_open(INADDR_ANY, 5513);
    for (;;) {
        if (udp_read(fd, buf, sizeof(buf), 0, &src_addr) < 0) {
            loge("socket recv fail: %s", strerror(errno));
            nsleep(0.1);
            continue;
        }
        logw("server read from '%s:%d': %s\n", inet_ntoa(GET_SOCKADDR(src_addr)), GET_SOCKPORT(src_addr), (char*)buf);

        logw("server write to '%s:%d': %s\n", inet_ntoa(GET_SOCKADDR(src_addr)), GET_SOCKPORT(src_addr), (char*)sinfo);
        udp_write(fd, sinfo, strlen(sinfo), 0, &src_addr);
    }
}

void* thread_udp_client(void *arg)
{
    char buf[100] = {0};
    const char *online = "CNMT online";
    struct sockaddr_in src_addr;
    struct sockaddr_in dst_addr;
    set_sockaddr(&dst_addr, INADDR_BROADCAST, 5513);

    int fd = udp_client_open();

    for (;;) {
        logi("client write to  '%s:%d': %s\n", inet_ntoa(GET_SOCKADDR(dst_addr)), GET_SOCKPORT(dst_addr), online);
        if (udp_write(fd, online, strlen(online), 0, &dst_addr) < 0) {
            loge("client send fail: %s", strerror(errno));
        }

        if (udp_read(fd, buf, sizeof(buf), 0, &src_addr) < 0) {
            loge("client read fail: %s", strerror(errno));
        }
        logi("client read from '%s:%d': %s\n\n", inet_ntoa(GET_SOCKADDR(src_addr)), GET_SOCKPORT(src_addr), (char*)buf);

        nsleep(1.0);
    }
}

int main(int argc, char **argv)
{
    if (common_init() != 0) {
        loge("common_init() fail: %d\n", errno);
        return 0;
    }

    cmdline = argparser_new(argc, argv);
    argparser_add(cmdline, "--aaa", 'a', 1);
    argparser_add(cmdline, "--bbb", 'b', 2);
    argparser_parse(cmdline, cmdline_proc);

    pthread_t pid;
    pthread_create(&pid, 0, thread_udp_server, 0);
    pthread_create(&pid, 0, thread_udp_client, 0);

    common_wait_exit();
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
    common_stop();
    argparser_delete(cmdline);
}

#ifdef __cplusplus
}
#endif

