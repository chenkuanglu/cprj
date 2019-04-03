/**
 * @file    app_init.c
 * @author  ln
 * @brief   application init
 **/

#include "app_init.h"

#ifdef __cplusplus
extern "C" {
#endif 

// print args
static void print_args(int argc, char **argv)
{
    if (argc <= 1 || argv == NULL) 
        return;

    printf("arguments  : ");
    for (int i=0; i<argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");
}

// application init
void app_init(start_info_t *sinfo)
{
#ifdef PRINT_STARTINFO
    printf("process id : %d\n", sinfo->pid);
    printf("up time    : %.6fs\n", sinfo->tm);
    print_args(sinfo->argc, sinfo->argv);
    printf("\n");
#endif
    // init log

    // your code here
    // ...
}

#ifdef __cplusplus
}
#endif 

