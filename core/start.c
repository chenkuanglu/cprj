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
void app_init(start_info_t *sinfo) 
{ 
    (void)sinfo;
    slogw(clog, "No extern app_init()\n");
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
    core_init();   
    clog = core_getlog();

    // all sub threads mask signal SIGINT(2) & SIGTERM(15) 
    sigemptyset(&mask_set);
    sigaddset(&mask_set, SIGINT);
    sigaddset(&mask_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask_set, NULL);

    if (thrq_init(&start_info.tq) != 0) {
        sloge(clog, "thrq_init() fail");
        return 0;
    }

    // create signal query thread
    if (pthread_create(&thr_sig, NULL, thread_sig, &start_info) != 0) {
        sloge(clog, "create 'thread_sig' fail");
        return 0;
    }

    // application init
    app_init(&start_info);

    int cmd;
    for (;;) {
        if (thrq_receive(&start_info.tq, &cmd, sizeof(cmd), 0) != 0) {
            nsleep(0.1);
            continue;
        }
        slogd(clog, "main thread get cmd: %d (0x%08x)", cmd, cmd);
        //thr_cancel_all(0);
        exit(0);
    }
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
            printf("\n");
            sprintf(reason, "catch signal '%s'", (sig == SIGTERM) ? "kill" : "Ctrl-C");
            slogn(clog, "exit, %s\n", reason);
            exit(0);    // send sig to main thread
        } else {
            sloge(clog, "'sigwait()' fail");
            nsleep(0.1);
        }
    }
}

#ifdef __cplusplus
}
#endif 

