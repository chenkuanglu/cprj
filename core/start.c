/**
 * @file    start.c
 * @author  ln
 * @brief   main function & start applicatoin
 **/

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif 

static start_info_t start_info;     // start info
static sigset_t mask_set;           // signal mask set
static pthread_t thr_sig;           // sync signal query thread id

static log_cb_t *clog;              // use core log to print

static void* thread_sig(void *arg);

__attribute__((weak)) 
int app_init(start_info_t *sinfo) 
{ 
    (void)sinfo;
    slogw(clog, "No extern app_init()\n");
    return 0;
}

__attribute__((weak)) 
void app_proper_exit(int ec) 
{ 
    (void)ec;
    slogw(clog, "No extern app_proper_exit()\n");
}

// main function
int main(int argc, char **argv)
{
    // start info
    start_info.argc = argc;
    start_info.argv = argv;
    start_info.pid  = getpid();
    start_info.tid  = pthread_self();
    start_info.tm   = monotime();
    
    // core init
    if (core_init() != 0) {
        loge("core_init() fail: %d\n", errno);    // 'clog' & 'err_string()' unavailable before 'core_init()'!
        return 0;
    }
    clog = core_getlog();

    // all sub threads mask signal SIGINT(2) & SIGTERM(15) 
    sigemptyset(&mask_set);
    sigaddset(&mask_set, SIGINT);
    sigaddset(&mask_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask_set, NULL);

    char ebuf[128];
    if (thrq_init(&start_info.tq) != 0) {
        sloge(clog, "thrq_init() init start_info.tq fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
        return 0;
    }

    // create signal query thread
    if (pthread_create(&thr_sig, NULL, thread_sig, &start_info) != 0) {
        sloge(clog, "pthread_create() create thread_sig fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
        return 0;
    }

    // application init
    if (app_init(&start_info) != 0) {
        sloge(clog, "app_init() fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
        return 0;
    }

    int ec;
    for (;;) {
        if (thrq_receive(&start_info.tq, &ec, sizeof(ec), 0) < 0) {
            sloge(clog, "thrq_receive() from start_info.tq fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
            nsleep(0.1);
            continue;
        }
        slogd(clog, "main thread get exit code: %d\n", ec);
        app_proper_exit(ec);
        core_proper_exit(ec);
        exit(0);
    }
}

void process_proper_exit(int ec)
{
    thrq_send(&start_info.tq, &ec, sizeof(ec));
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
            slogn(clog, "exit, %s\n", reason);
            process_proper_exit(0);
        } else {
            sloge(clog, "'sigwait()' fail\n");
            nsleep(0.1);
        }
    }
}

#ifdef __cplusplus
}
#endif 

