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
    if (log_init(&core_info.log) != 0) 
        return -1;
    core_log = &core_info.log;

    if (pthread_create(&core_info.thr_guard, NULL, thread_guard, &core_info) != 0) {
        sloge(CLOG, "pthread_create() create thread_sig fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
        return -1;
    }

    return 0;
}

static void* thread_guard(void *arg)
{
    (void)arg;
    pthread_t tid = pthread_self();
    pthread_detach(tid);
    uint32_t count = 0;

    for (;;) {
        nsleep(0.1);
        count++;
        if (count == 10) {
            slogd(CLOG, "1s guard...\n");
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

