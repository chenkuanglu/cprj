#include <stdio.h>
#include <time.h>
#include "threads_c11.h"
#include "timetick.h"

int main()
{
    cnd_t cond;
    mtx_t mtx;
    struct timespec tm;

    mtx_init(&mtx, mtx_plain);
    cnd_init(&cond);
    timespec_get(&tm, TIME_MONO);
    //cnd_init(&cond, TIME_UTC);
    //timespec_get(&tm, TIME_UTC);

    tm.tv_sec += 2;
    printf("timespec_get_r() sec=%ld\n", tm.tv_sec);

    mtx_lock(&mtx);
    printf("cnd_timedwait() 2s ... [time=%ld]\n", time(0));
    cnd_timedwait(&cond, &mtx, &tm);
    printf("ok, timeout!           [time=%ld]\n", time(0));

    mtx_unlock(&mtx);

    printf("start sleep ...     [time=%f]\n", monotime());
    nsleep(1.5);
    printf("end sleep ...       [time=%f]\n", monotime());

    struct tm tt;
    time_t key = time(NULL);
    char buf[100];
    time2local(&tt, &key);
    time2str(buf, &tt, 100-1);
    str2local(&tt, buf);
    printf("org time=%ld, time2str=%s, str2local=%ld\n", key, buf, mktime(&tt));

    getchar();
    return 0;
}

