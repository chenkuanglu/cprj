 

#define CORE_MSG_TYPE_BASE      ( -1 )
#define CORE_MSG_CMD_BASE       ( -256 )

#define CORE_MSG_TYPE_TIMER     ( CORE_MSG_TYPE_BASE - 1 )

#define CORE_MSG_CMD_EXPIRE     ( CORE_MSG_CMD_BASE - 1 )

typedef struct {
    long    type;
    long    cmd;
    double  tm;
    long    len;
    char    data[];
} core_msg_t;


extern int          core_msg_send(thrq_cb_t *thrq, int type, int cmd, void *data, size_t len);
extern core_msg_t*  core_msg_recv(thrq_cb_t *thrq, void *buf, size_t size);
extern int          core_msg_count(thrq_cb_t *thrq);

int core_msg_send(thrq_cb_t *thrq, int type, int cmd, void *data, size_t len)
{
#define CORE_MAX_MSG_LEN    (1024 + sizeof(core_msg_t))

    if (len > CORE_MAX_MSG_LEN) {
        errno = LIB_ERRNO_BUF_SHORT;
        return -1;
    }

    char buf[CORE_MAX_MSG_LEN];
    core_msg_t *cmsg = (core_msg_t *)buf;
    cmsg->type = type;
    cmsg->cmd = cmd;
    cmsg->tm = monotime();
    if (data == NULL || len == 0) {
        len = 0;
    } else {
        memcpy(cmsg->data, data, len);
    }
    cmsg->len = len;

    return thrq_send(thrq, &cmsg, sizeof(core_msg_t)+len);
}

core_msg_t* core_msg_recv(thrq_cb_t *thrq, void *buf, size_t size)
{
    if (thrq == NULL || buf == NULL || size == 0) {
        return NULL;
    }
    if (thrq_receive(thrq, buf, size, 0) < 0) {
        return NULL;
    }
    return (core_msg_t *)buf;
}

int core_msg_count(thrq_cb_t *thrq)
{
    return thrq_count(thrq);
}

==================================================

void* thread_udp_client(void *arg)
{
    char buf[100] = {0};
    const char *online = "CNMT online";
    struct sockaddr_in src_addr;
    struct sockaddr_in dst_addr;
    net_setsaddr(&dst_addr, INADDR_BROADCAST, 5513);

    int fd = udp_open(0, 0, 0);
    for (;;) {
        logi("client write to  '%s:%d': %s\n", inet_ntoa(NET_SOCKADDR(dst_addr)), NET_SOCKPORT(dst_addr), online);
        if (udp_write(fd, online, strlen(online), 0, &dst_addr) < 0) {
            loge("client send fail: %s", strerror(errno));
        }

        if (udp_read(fd, buf, sizeof(buf), 0, &src_addr) < 0) {
            loge("client read fail: %s", strerror(errno));
        }
        logi("client read from '%s:%d': %s\n\n", inet_ntoa(NET_SOCKADDR(src_addr)), NET_SOCKPORT(src_addr), (char*)buf);

        nsleep(1.0);
    }
}

void* thread_udp_server(void *arg)
{
    char buf[100] = {0};
    const char *sinfo = "server info";
    struct sockaddr_in src_addr;

    int fd = udp_open(0, 0, 0);
    udp_bind(fd, INADDR_ANY, 5513);
    for (;;) {
        if (udp_read(fd, buf, sizeof(buf), 0, &src_addr) < 0) {
            loge("socket recv fail: %s", strerror(errno));
            nsleep(0.1);
            continue;
        }
        logw("server read from '%s:%d': %s\n", inet_ntoa(NET_SOCKADDR(src_addr)), NET_SOCKPORT(src_addr), (char*)buf);

        logw("server write to '%s:%d': %s\n", inet_ntoa(NET_SOCKADDR(src_addr)), NET_SOCKPORT(src_addr), (char*)sinfo);
        udp_write(fd, sinfo, strlen(sinfo), 0, &src_addr);
    }
}



    //pthread_t pid;
    //pthread_create(&pid, 0, thread_udp_server, 0);
    //pthread_create(&pid, 0, thread_udp_client, 0);

