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

log_cb_t *init_log;

#ifdef PRINT_ARGS
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
    if ((buf = (char *)malloc(sum+1)) == NULL)
        return;
    num = sprintf(buf, "%s", pre);
    for (int i=0; i<argc; i++) {
        num += sprintf(buf+num, "%s ", argv[i]);
    }
    slogi(init_log, "%s", buf);
    free(buf);
}
#endif

// application init
void app_init(start_info_t *sinfo)
{
    init_log = CLOG;
    slogd(init_log, "app init...\n");

#ifdef PRINT_ARGS
    print_args(sinfo->argc, sinfo->argv);
    printf("\n");
#endif
    
    trigg_init(sinfo);
}

void app_proper_exit(int ec)
{
    slogd(init_log, "app_proper_exit(%d)...\n", ec);
}

#ifdef __cplusplus
}
#endif 

