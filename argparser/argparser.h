/**
 * @file    argparser.c
 * @author  ln
 * @brief   命令行参数解析器(解析main函数的argv)\n
 *          解析器创建后，需要通过argparser_add注册指定的选项名称(-h)和参数个数，\n
 *          最后调用argparser_parse执行所有参数的解析
 */

#ifndef __ARG_PARSER_H__
#define __ARG_PARSER_H__

#include "../lib/que.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int         argc;       ///< 参数个数
    char**      argv;       ///< 参数数组
    que_cb_t*   opt_names;  ///< 选项的名称，例如 -h --help
} argparser_t;

typedef int (*parse_callback_t)(long id, char **param, int num);

extern argparser_t* argparser_new(int argc, char **argv);
extern void         argparser_delete(argparser_t* parser);

extern int          argparser_add(argparser_t *parser, const char* arg_name, long arg_id, int param_num);
extern int          argparser_parse(argparser_t *parser, parse_callback_t parse_proc);

#ifdef __cplusplus
}
#endif

#endif

