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
    if ((buf = (char *)malloc(sum+argc+1)) == NULL)     // space between argv[i]
        return;
    num = sprintf(buf, "%s", pre);
    for (int i=0; i<argc; i++) {
        num += sprintf(buf+num, "%s ", argv[i]);
    }
    slogi(CLOG, "%s\n", buf);
    free(buf);
}
#endif

// application init
void app_init(start_info_t *sinfo)
{
    slogd(CLOG, "app init...\n");

#ifdef PRINT_ARGS
    print_args(sinfo->argc, sinfo->argv);
    printf("\n");
#endif
    
    trigg_init(sinfo);
}

void app_proper_exit(int ec)
{
    slogd(CLOG, "app_proper_exit(%d)...\n", ec);
}

#ifdef __cplusplus
}
#endif 

