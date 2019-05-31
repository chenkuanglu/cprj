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
    log_cb_t    log;            // core log object

    thrq_cb_t   thrq_start;
    pthread_t   tid_start;      // thread id of main thread

    pthread_t   tid_sig;        // sync signal query thread id
} core_info_t;

static core_info_t core_info;
static sigset_t mask_set;

log_cb_t *CLOG = NULL;

static void* thread_sig(void *arg);

int core_init(int argc, char **argv)
{
    memset(&core_info, 0, sizeof(core_info_t));

    // init error-code module
    if (err_init() != 0) 
        return -1;
    // create log
    if (log_init(&core_info.log) != 0) 
        return -1;
    CLOG = &core_info.log;
    // create thrq
    if (thrq_init(&core_info.thrq_start) != 0) {
        return -1;
    }

    // all sub threads mask signal SIGINT(2) & SIGTERM(15) 
    sigemptyset(&mask_set);
    sigaddset(&mask_set, SIGINT);
    sigaddset(&mask_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask_set, NULL);
    // create signal query thread
    if (pthread_create(&core_info.tid_sig, NULL, thread_sig, &core_info) != 0) {
        return 0;
    }

    // init timer
    if (TMR_INIT() != 0) 
        return -1;
    if (TMR_START() != 0) 
        return -1;

    return 0;
}

// wait exit code
int core_wait_exit(void)
{
    int ec;
    char ebuf[128];
    core_info.tid_start = pthread_self();

    for (;;) {
        if (thrq_receive(&core_info.thrq_start, &ec, sizeof(ec), 0) < 0) {
            sloge(CLOG, "fail to wait exit code: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
            nsleep(0.1);
            continue;
        }
    }

    return ec;
}

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
    cmsg->len = len;
    memcpy(cmsg->data, data, len);

    thrq_send(qsend, &cmsg, sizeof(core_msg_t)+len);
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

void core_stop(void)
{
    TMR_STOP();
}

void core_exit(int ec)
{
    thrq_send(&core_info.thrq_start, &ec, sizeof(ec));
    CORE_THR_RETIRE();
}

// thread signal sync process, SIGTERM & SIGINT should be mask in any thread!
static void* thread_sig(void *arg)
{
    (void)arg;
    int sig, rc;
    sigset_t sig_set;
    char reason[64] = {0};

    pthread_t tid = pthread_self();
    pthread_detach(tid);

    sigemptyset(&sig_set);
    sigaddset(&sig_set, SIGTERM);
    sigaddset(&sig_set, SIGINT);

    for (;;) {
        rc = sigwait(&sig_set, &sig);
        if (rc == 0) {
            printf("\n");   // ^C
            sprintf(reason, "killed by signal '%s'", (sig == SIGTERM) ? "SIGTERM" : "SIGINT");
            slogn(CLOG, "exit, %s\n", reason);
            core_proper_exit(0);
        } else {
            sloge(CLOG, "'sigwait()' fail\n");
            nsleep(0.1);
        }
    }
}

#ifdef __cplusplus
}
#endif 

