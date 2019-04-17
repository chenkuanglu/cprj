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
    start_info_t start_info;    // start info

    log_cb_t    log;            // core log object

    thrq_cb_t   thrq_start;
    pthread_t   tid_start;      // thread id of main thread
    pthread_t   tid_sig;        // sync signal query thread id

    pthread_t   tid_guard;
    que_cb_t    que_guard;
} core_info_t;

static core_info_t core_info;
static sigset_t mask_set;

log_cb_t *core_log;

static void* thread_guard(void *arg);
static void* thread_sig(void *arg);

__attribute__((weak)) 
int app_init(start_info_t *sinfo) 
{ 
    slogw(CLOG, "No extern app_init()\n");
    return 0;
}

__attribute__((weak)) 
void app_proper_exit(int ec) 
{ 
    slogw(CLOG, "No extern app_proper_exit()\n");
}

int core_init(start_info_t *sinfo)
{
    char ebuf[128];
    memset(&core_info, 0, sizeof(core_info_t));

    // start info
    core_info.start_info.argc = sinfo->argc;
    core_info.start_info.argv = sinfo->argv;
    core_info.start_info.tm   = sinfo->tm;
    
    if (err_init() != 0) 
        return -1;

    if (log_init(&core_info.log) != 0) 
        return -1;
    core_log = &core_info.log;

    if (thrq_init(&core_info.thrq_start) != 0) {
        sloge(CLOG, "thrq_init() init start_info.tq fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
        return -1;
    }

    // all sub threads mask signal SIGINT(2) & SIGTERM(15) 
    sigemptyset(&mask_set);
    sigaddset(&mask_set, SIGINT);
    sigaddset(&mask_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask_set, NULL);

    // create signal query thread
    if (pthread_create(&core_info.tid_sig, NULL, thread_sig, &core_info) != 0) {
        sloge(CLOG, "pthread_create() create thread_sig fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
        return 0;
    }

    if (que_init(&core_info.que_guard) != 0) 
        return -1;
    que_set_mpool(&core_info.que_guard, 0, sizeof(thrq_cb_t *));

    if (pthread_create(&core_info.tid_guard, NULL, thread_guard, &core_info) != 0) {
        sloge(CLOG, "pthread_create() create thread_sig fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
        return -1;
    }

    app_init(&core_info.start_info);
    
    return 0;
}

static void __core_stall(void)
{
    // stop timer sig to app
}

static void __core_exit(int ec)
{
    exit(0);
}

// main loop
void core_loop(void)
{
    int ec;
    char ebuf[128];

    core_info.tid_start = pthread_self();

    for (;;) {
        if (thrq_receive(&core_info.thrq_start, &ec, sizeof(ec), 0) < 0) {
            sloge(CLOG, "thrq_receive() from start_info.tq fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
            nsleep(0.1);
            continue;
        }
        slogd(CLOG, "main thread get exit code: %d\n", ec);
        __core_stall();    
        app_proper_exit(ec);
        __core_exit(ec);
    }
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
            sprintf(reason, "Catch signal '%s'", (sig == SIGTERM) ? "Kill" : "Ctrl-C");
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

