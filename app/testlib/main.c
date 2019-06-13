/**
 * @file    main.c
 * @author  ln
 * @brief   main function
 **/

#include "../../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif 

// main function
int main(int argc, char **argv)
{
    // core init
    if (core_init(argc, argv) != 0) {
        loge("core_init() fail: %d\n", errno);
        return 0;
    }

    logi("test thread queue...\n");

    core_wait_exit();
}

void app_proper_exit(int ec)
{
    // app_stop1(...);
    core_stop();
    // app_stop2(...);
    logw("testlib exit, code %d\n", ec);
}

#ifdef __cplusplus
}
#endif 

