/**
 * @file    app_init.c
 * @author  ln
 * @brief   application init
 **/

#include "app_init.h"
#include "trigg_miner.h"

#ifdef __cplusplus
extern "C" {
#endif 

static void print_args(int argc, char **argv)
{
    int sum, num;
    char *buf;
    const char *pre = "Arguments: ";
    if (argc <= 1 || argv == NULL) 
        return;
    sum = strlen(pre);
    for (int i=0; i<argc; i++) {
        sum += strlen(argv[i]);
    }
    if ((buf = (char *)malloc(sum+argc+1)) == NULL)     // space between argv[i]
        return;
    num = sprintf(buf, "%s", pre);
    for (int i=0; i<argc; i++) {
        num += sprintf(buf+num, "%s ", argv[i]);
    }
    slogi(CLOG, "%s\n", buf);
    free(buf);
}

// application init
int main(int argc, char **argv)
{
    print_args(argc, argv);
    printf("\n");
    
    if (core_init(argc, argv) != 0) {
        loge("core_init() fail: %d\n", errno);
        return 0;
    }
    if (trigg_init(argc, argv) != 0) {
        core_exit(0);
    }

    core_wait_exit();
}

void app_proper_exit(int ec)
{
    core_stop();
    slogd(CLOG, "trigg exit, code %d\n", ec);
}

#ifdef __cplusplus
}
#endif 

