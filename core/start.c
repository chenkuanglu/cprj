/**
 * @file    start.c
 * @author  ln
 * @brief   main function
 **/

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif 

// main function
int main(int argc, char **argv)
{
    start_info_t info;

    info.argc = argc;
    info.argv = argv;
    info.tm = monotime();

    // core init
    if (core_init(&info) != 0) {
        loge("core_init() fail: %d\n", errno);
        return 0;
    }

    core_loop();
}

#ifdef __cplusplus
}
#endif 

