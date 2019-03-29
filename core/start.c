/**
 * @file    start.c
 * @author  ln
 * @brief   main function & start applicatoin
 **/

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif 

// start info
static start_info_t start_info;
// signal mask set
static sigset_t mask_set;
// sync signal query thread id
pthread_t thr_sig;

static void* sig_thread(void *arg);

// main function
int main(int argc, char **argv)
{
    // all sub threads mask signal SIGINT(2) & SIGTERM(15) 
    sigemptyset(&mask_set);
    sigaddset(&mask_set, SIGINT);
    sigaddset(&mask_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask_set, NULL);

    // start info
    start_info.argc = argc;
    start_info.argv = argv;
    start_info.pid  = getpid();
    start_info.tid  = pthread_self();
    start_info.tm   = monotime();

    // create signal query thread
    if (pthread_create(&thr_sig, NULL, sig_thread, &start_info) != 0) {
        perror("error, 'sig_thread' create");
    }

    // application init
    app_init(&start_info);

    double slp = 1.5;
    for (;;) {
        printf("thrsleep: %.6fs...\n", slp);
        thrsleep(slp);
    }
}

// thread signal sync process, SIGTERM & SIGINT should be mask in any thread!
static void* sig_thread(void *arg)
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
            sprintf(reason, "catch signal '%s'", (sig == SIGTERM) ? "kill" : "ctrl-c");
            printf("exit, reason: %s\n", reason);
            exit(0);    // send sig to main thread
        } else {
            perror("error, 'sigwait()'");
        }
    }
}

#ifdef __cplusplus
}
#endif 

