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
} core_info_t;

static core_info_t core_info;

log_cb_t* core_getlog(void)
{
    return &core_info.log;
}

int core_init(void)
{
    memset(&core_info, 0, sizeof(core_info_t));

    if (err_init() != 0) 
        return -1;
    if (log_init(&core_info.log) != 0) 
        return -1;
    return 0;
}

void core_proper_exit(int ec)
{
    sloge(&core_info.log, "core_proper_exit(%d)...\n", ec);
    //thrq_send() to main thread
}

int thr_new(const char *name, pthread_t *tid, const pthread_attr_t *attr, core_thread_t fn, void *arg)
{
    (void)name;
    return pthread_create(tid, attr, fn, arg);
}

#ifdef __cplusplus
}
#endif 

