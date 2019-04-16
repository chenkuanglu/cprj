/**
 * @file    core.c
 * @author  ln
 * @brief   core 
 **/

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif 

// core info
typedef struct {
    log_cb_t    log;        // core log object
    pthread_t   thr_guard;

    que_cb_t    que_guard;
} core_info_t;

static core_info_t core_info;
log_cb_t *core_log;
static void* thread_guard(void *arg);

log_cb_t* core_getlog(void)
{
    return &core_info.log;
}

int core_init(void)
{
    char ebuf[128];
    memset(&core_info, 0, sizeof(core_info_t));

    if (err_init() != 0) 
        return -1;

    if (que_init(&core_info.que_guard) != 0) 
        return -1;
    que_set_mpool(&core_info.que_guard, 0, 32);

    if (log_init(&core_info.log) != 0) 
        return -1;
    core_log = &core_info.log;

    if (pthread_create(&core_info.thr_guard, NULL, thread_guard, &core_info) != 0) {
        sloge(CLOG, "pthread_create() create thread_sig fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
        return -1;
    }

    return 0;
}

int core_add_guard(thrq_cb_t *thrq)
{
    int res = que_insert_tail(&core_info.que_guard, &thrq, sizeof(thrq_cb_t *));
    return res;
}

static void* thread_guard(void *arg)
{
    (void)arg;
    pthread_t tid = pthread_self();
    pthread_detach(tid);
    uint32_t count = 0;
    que_cb_t *pq = &core_info.que_guard;
    thrq_cb_t **qsend;

    core_msg_t cmsg;
    cmsg.type = CORE_MSG_TYPE_TIMER;
    cmsg.cmd = CORE_MSG_CMD_EXPIRE;
    cmsg.tm = monotime();
    cmsg.len = 0;

    for (;;) {
        nsleep(0.1);
        count++;
        if (count % 10 == 0) {
            que_elm_t *var;
            QUE_FOREACH(var, pq) {
                qsend = (thrq_cb_t **)var->data;
                thrq_send(*qsend, &cmsg, sizeof(cmsg));
            }    
        }
    }
}

void core_proper_exit(int ec)
{
    slogd(&core_info.log, "core_proper_exit(%d)...\n", ec);
}

#ifdef __cplusplus
}
#endif 

