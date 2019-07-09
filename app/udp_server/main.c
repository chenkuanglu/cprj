/**
 * @file    main.c
 * @author  ln
 * @brief   app 模板工程
 */

#include "../../common/common.h"
#include "udp_server.h"

#ifdef __cplusplus
extern "C" {
#endif

int args_argc = 0;
char **args_argv = 0;

// 主函数
int main(int argc, char **argv)
{
    char err_buf[128];
    pthread_t tid;
    
    args_argc = argc;
    args_argv = argv;
    
    // init common modules
    if (common_init() != 0) {
        printf(CCL_RED "init fail: %s\n" CCL_END, err_string(errno, err_buf, sizeof(err_buf)));
        return EXIT_FAILURE;
    }

    // create thread for app init        
    pthread_create(&tid, 0, thread_init, 0);
    
    // waiting for exit-msg
    common_wait_exit();
}

#ifdef __cplusplus
}
#endif

