/**
 * @file    main.c
 * @author  ln
 * @brief   app 模板工程
 *
 * run: build/app/template/template --opt1 a1 --opt2 b2 c3
 */

#include "../../common/common.h"
#include "user_app.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *program_name = MAKE_CSTR(PROGRAM_NAME);
argparser_t *cmdl_psr = NULL;
int cmdline_proc(long id, char **param, int num);

const char usage[] = "\
Usage: " MAKE_CSTR(PROGRAM_NAME) " [Options] [Args]\n\
Options:\n\
    -v  --version   打印软件版本号\n\
    -h              打印帮助信息\n\
";

// app入口函数
int main(int argc, char **argv)
{
    char err_buf[128];
    if (common_init() != 0) {
        printf("init fail: %s\n", err_string(errno, err_buf, sizeof(err_buf)));
        return EXIT_FAILURE;
    }

    if ((cmdl_psr = argparser_new(argc, argv)) == NULL) {
        loge("parse fail: %s\n", err_string(errno, err_buf, sizeof(err_buf)));
        return EXIT_FAILURE;
    }     
    argparser_add(cmdl_psr, "--version", 'v', 0);
    argparser_add(cmdl_psr, "-v", 'v', 0);   
    argparser_add(cmdl_psr, "-h", 'h', 0);
    argparser_parse(cmdl_psr, cmdline_proc);

    logn("%s version %d.%d.%d\n", program_name, 
                    VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
                  
    app_init();
    
    common_wait_exit();
}

// 命令行参数的处理函数
int cmdline_proc(long id, char **param, int num)
{
    switch (id) {
        case 'v':
            printf("%s version %d.%d.%d\n", program_name, 
                    VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
            exit(EXIT_SUCCESS);
            break;
            
        case 'h':
            printf("%s\n", usage);
            exit(EXIT_SUCCESS);
            break;
            
        default:
            exit(EXIT_FAILURE);
            break;
    }
    return 0;
}

void app_proper_exit(int ec)
{
    common_stop();
    argparser_delete(cmdl_psr);
}

#ifdef __cplusplus
}
#endif

