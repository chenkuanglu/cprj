/**
 * @file    main.c
 * @author  ln
 * @brief   main function
 **/

#include "../../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif 

argparser_t *cmdline;
int parse_callback(long id, char **param, int num)
{
    logw("proc '%c', num=%d\n", (char)id, num);
    for (int i=0; i<num; i++) {
        logi("'%s'\n", param[i]);
    }
    return 0;
}

// main function
int main(int argc, char **argv)
{
    // core init
    if (core_init(argc, argv) != 0) {
        loge("core_init() fail: %d\n", errno);
        return 0;
    }

    cmdline = argparser_new(argc, argv);
    argparser_add(cmdline, "--aaa", 'a', 0);
    argparser_add(cmdline, "--bbb", 'b', 0);
    argparser_add(cmdline, "--ccc", 'c', 0);
    argparser_parse(cmdline, parse_callback);

    core_wait_exit();
}

void app_proper_exit(int ec)
{
    core_stop();
    argparser_delete(cmdline);
}

#ifdef __cplusplus
}
#endif 

