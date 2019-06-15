/**
 * @file    argparser.h
 * @author  ln
 * @brief   program arguments parser
 **/

#ifndef __ARG_PARSER_H__
#define __ARG_PARSER_H__

#include "../lib/que.h"
#include "../lib/log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int         argc;
    char**      argv;
    que_cb_t*   arg_names;
} argparser_t;

typedef int (*parse_callback_t)(long id, char **param, int num);

extern argparser_t* argparser_new       (int argc, char **argv);
extern void         argparser_delete    (argparser_t* parser);

extern int          argparser_add       (argparser_t *parser, const char* arg_name, long arg_id, int param_num);
extern int          argparser_parse     (argparser_t *parser, parse_callback_t parse_proc);

#ifdef __cplusplus
}
#endif

#endif

