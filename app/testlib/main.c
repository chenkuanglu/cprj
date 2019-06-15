/**
 * @file    main.c
 * @author  ln
 * @brief   main function
 *
 * run: build/app/testlib/testlib --aaa 1 --bbb 1 0x12 3
 **/

#include "../../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif 

argparser_t *cmdline;
int cmdline_proc(long id, char **param, int num);

int main(int argc, char **argv)
{
    if (core_init(argc, argv) != 0) {
        loge("core_init() fail: %d\n", errno);
        return 0;
    }

    cmdline = argparser_new(argc, argv);
    argparser_add(cmdline, "--aaa", 'a', 1);
    argparser_add(cmdline, "--bbb", 'b', 2);
    argparser_parse(cmdline, cmdline_proc);

    core_wait_exit();
}

int cmdline_proc(long id, char **param, int num)
{
    switch (id) {
        case 'a':
            logi("'id=%c',n=%d,(%d)\n", id, num, strtol(param[0], 0, 0));
            break;
        case 'b':
            logi("'id=%c',n=%d,(%d %d)\n", id, num, strtol(param[0], 0, 0), strtol(param[1], 0, 0));
            break;
        default:
            break;
    }
    return 0;
}

void app_proper_exit(int ec)
{
    core_stop();
    argparser_delete(cmdline);
}

#ifdef __cplusplus
}
#endif 

