/**
 * @file    start.c
 * @author  ln
 * @brief   main function & start applicatoin
 **/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include "app/app_init.h"

#define PRINT_START_INFO    1

static start_info_t start_info;
static sigset_t mask_set;
pthread_t thr_sig;

void* sig_thread(void *arg);

#if PRINT_START_INFO != 0
// print argv
static void print_argv(int argc, char **argv)
{
    if (argc <= 1 || argv == NULL) 
        return;

    printf("arguments: ");
    for (int i=0; i<argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");
}
#endif

int main(int argc, char **argv)
{
    // all sub threads mask signal SIGINT(2) & SIGTERM(15) 
    sigemptyset(&mask_set);
    sigaddset(&mask_set, SIGINT);
    sigaddset(&mask_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask_set, NULL);

    start_info.argc = argc;
    start_info.argv = argv;
    start_info.pid  = getpid();
    start_info.tid  = pthread_self();

#if PRINT_START_INFO != 0
    printf("process id: %d\n", start_info.pid);
    print_argv(argc, argv);
    printf("\n");
#endif

    if (pthread_create(&thr_sig, 0, sig_thread, 0) != 0) {
        perror("error, 'sig_thread' create");
    }

    // create core thread ...
    
    app_init(&start_info);

    struct timespec tms = {1, 0};
    for (;;) {
        printf("nanosleep: 1s...\n");
        nanosleep(&tms, 0);
    }
}

// thread signal sync process, SIGTERM & SIGINT should be mask in any thread
void* sig_thread(void *arg)
{
    int sig, rc;
    sigset_t sig_set;
    char reason[64] = {0};

    (void)arg;
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
            exit(0);
        } else {
            perror("error, 'sigwait()'");
        }
    }
}

