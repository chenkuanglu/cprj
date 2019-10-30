#include <stdio.h>
#include <time.h>
#include "threads_c11.h"

int main()
{
    cnd_t cond;
    mtx_t mtx;
    struct timespec tm;

    mtx_init(&mtx, mtx_plain);
    cnd_init_r(&cond, TIME_MONO);
    timespec_get_r(&tm, TIME_MONO);
    //cnd_init_r(&cond, TIME_UTC);
    //timespec_get_r(&tm, TIME_UTC);

    tm.tv_sec += 2;
    printf("timespec_get_r() sec=%ld\n", tm.tv_sec);

    mtx_lock(&mtx);
    printf("cnd_timedwait() 2s ... [time=%ld]\n", time(0));
    cnd_timedwait(&cond, &mtx, &tm);
    printf("ok, timeout!           [time=%ld]\n", time(0));
    mtx_unlock(&mtx);

    getchar();
    return 0;
}

