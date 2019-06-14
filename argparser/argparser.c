/**
 * @file    argparser.c
 * @author  ln
 * @brief   program arguments parser
 **/

#include "argparser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ARG_NAME    64

typedef struct {
    char    name[MAX_ARG_NAME];     // option name such as '-h', '--help', 'set', 'W'
    long    id;                     // id for callback proc
    long    n;                      // the number of args at least
} argparser_args_t;

static int argcmp(const void* left, const void* right, size_t len)
{
    if (left != NULL && right != NULL && len > 0) {
        const argparser_args_t *l = (const argparser_args_t *)left;
        const argparser_args_t *r = (const argparser_args_t *)right;
        if (strcmp(l->name, r->name) == 0)  {
            loge("argparser: multiple arg name '%s'\n", l->name);
            return 0;
        }
    }

    return -1;
}

argparser_t* argparser_new(int argc, char **argv)
{
    if (argc <= 0 || argv == NULL)
        return NULL;

    argparser_t* p = (argparser_t *)malloc(sizeof(argparser_t));
    if (p != NULL) {
        p->arg_names = que_new(NULL);
        if (p->arg_names != NULL) {
            p->argc = argc;
            p->argv = argv;
            return p;
        }
        else 
            free(p);
    }
    return NULL;
}

void argparser_delete(argparser_t* parser)
{
    if (parser != NULL) {
        if (parser->arg_names != NULL) {
            que_destroy(parser->arg_names);
        }
        free(parser);
    }
}

void argparser_add(argparser_t *parser, const char* arg_name, long arg_id, int param_num)
{
    if (parser == NULL || arg_name == NULL || arg_id <= 0 || param_num < 0) {
        loge("argparser: Invalid parameter\n");
        return;
    }
    if (strlen(arg_name) > MAX_ARG_NAME - 1) {
        loge("argparser: Arg name too long\n");
        return;
    }

    argparser_args_t arg;
    strcpy(arg.name, arg_name);
    arg.id = arg_id;
    arg.n = param_num;
    QUE_LOCK(parser->arg_names);
    if (QUE_FIND(parser->arg_names, &arg, sizeof(argparser_args_t), argcmp ) == NULL) {
        if (que_insert_head(parser->arg_names, &arg, sizeof(argparser_args_t)) < 0)
            loge("argparser: Fail to add, cannot insert queue\n");
    }
    QUE_UNLOCK(parser->arg_names);
}

int argparser_parse(argparser_t *parser, parse_callback_t parse_proc)
{
    if (parser == NULL || parse_proc == NULL)
        return -1;

    char **prev_v = NULL, **param = NULL;
    int prev_c;
    int prev_n;
    int prev_i;

    int found;
    int c = parser->argc - 1;
    char **v = parser->argv + 1;
    QUE_LOCK(parser->arg_names);
    while (c > 0) {
        que_elm_t *var;
        argparser_args_t *arg = NULL;
        QUE_FOREACH_REVERSE(var, parser->arg_names) {
            arg = (argparser_args_t *)var->data;
            if ((found = strcmp(*v, arg->name)) == 0) { 
                prev_n = arg->n;
                /* process the previous opt */
                if (prev_v != NULL) {
                    if (prev_c < prev_n) {
                        loge("argparser: Fail to parse '%s', Short of parameter\n", *prev_v);
                        QUE_UNLOCK(parser->arg_names);
                        return -1;
                    }
                    if (prev_c == 0)
                        param = NULL;
                    else 
                        param = prev_v+1;
                    if (parse_proc(prev_i, param, prev_c) < 0) {         /* number of arg may be more than 'arg->n' */
                        QUE_UNLOCK(parser->arg_names);
                        return 0;                                        /* user stop parse */
                    }
                }

                prev_v = v;
                prev_i = arg->id;

                prev_c = 0;
                c--;
                v++;
                break;
            }
        }
        if (found != 0) {
            prev_c++;
            c--;
            v++;
        }
    }

    /* process the last opt */
    if (prev_v != NULL) {
        if (prev_c < prev_n) {
            loge("argparser: Fail to parse '%s', Short of parameter\n", *prev_v);
            QUE_UNLOCK(parser->arg_names);
            return -1;
        }
        if (prev_c == 0)
            param = NULL;
        else 
            param = prev_v+1;
        parse_proc(prev_i, param, prev_c);
    }
    QUE_UNLOCK(parser->arg_names);

    return 0;
}

#ifdef __cplusplus
}
#endif

