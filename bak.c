 

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



