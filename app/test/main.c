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

    core_wait_exit();
}

#ifdef __cplusplus
}
#endif 

