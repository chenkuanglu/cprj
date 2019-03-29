/**
 * @file    app_init.c
 * @author  ln
 * @brief   application init
 **/

#include "app_init.h"

#ifdef __cplusplus
extern "C" {
#endif 

// print argv
#ifdef PRINT_STARTINFO
static void print_args(int argc, char **argv)
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

// application init
void app_init(start_info_t *sinfo)
{
#ifdef PRINT_STARTINFO
    printf("Process id: %d\n", sinfo->pid);
    printf("Up time: %.6fs\n", sinfo->tm);
    printf("\n");
    print_args(sinfo->argc, sinfo->argv);
#endif
    // init log

    // your code here
    // ...
}

#ifdef __cplusplus
}
#endif 

