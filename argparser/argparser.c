/**
 * @file    argparser.c
 * @author  ln
 * @brief   命令行参数解析器(解析main函数的argv)\n
 *          解析器创建后，需要通过argparser_add注册指定的选项名称(-h)和参数个数，\n
 *          最后调用argparser_parse执行所有参数的解析
 */

#include "../lib/log.h"
#include "argparser.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ARG_NAME    64          ///< 选项名的最大长度

typedef struct {
    char    name[MAX_ARG_NAME];     ///< 选项名，例如'-h', '--help', 'set', 'W'
    long    id;                     ///< 该选项的ID号，用于回调进行识别
    long    n;                      ///< 该选项所需的最少参数个数
} argparser_args_t;


/**
 * @brief   内部函数，比较选项名称是否相同
 * @param   left    比较对象1
 * @param   right   比较对象2
 * @param   len     比较长度
 *
 * @return  相同返回0，不相同或者错误返回-1
 */
static int argcmp(const void* left, const void* right, size_t len)
{
    if (left != NULL && right != NULL && len > 0) {
        const argparser_args_t *l = (const argparser_args_t *)left;
        const argparser_args_t *r = (const argparser_args_t *)right;
        if (strcmp(l->name, r->name) == 0)  {
            return 0;
        }
    }

    return -1;
}

/**
 * @brief   新建解析参数的对象
 * @param   argc    来自命令行参数argc
 * @param   argv    来自命令行参数argv
 *
 * @return  成功返回对象指针，失败返回NULL
 *
 * @attention   返回的指针需要使用argparser_delete释放，而不是直接free
 */
argparser_t* argparser_new(int argc, char **argv)
{
    if (argc <= 0 || argv == NULL) {
        errno = EINVAL;
        return NULL;
    }

    argparser_t* p = (argparser_t *)malloc(sizeof(argparser_t));
    if (p != NULL) {
        p->opt_names = que_new(NULL, NULL);
        if (p->opt_names != NULL) {
            p->argc = argc;
            p->argv = argv;
            return p;
        }
        else 
            free(p);
    }
    return NULL;
}

/**
 * @brief   销毁并释放参数解析器
 * @param   parser  参数解析器
 * @return  void
 * @attention   delete操作将连同解析器对象一起释放（有别于destroy操作）
 */
void argparser_delete(argparser_t* parser)
{
    if (parser != NULL) {
        if (parser->opt_names != NULL) {
            que_destroy(parser->opt_names);
            free(parser->opt_names);
        }
        free(parser);
    }
}

/**
 * @brief   内部函数，判断选项是否已经存在
 * @param   parser      参数解析器
 * @param   opt_name    选项名称
 *
 * @return  成功返回选项指针，失败返回NULL
 */
static argparser_args_t* argparser_exist(argparser_t *parser, const char* opt_name)
{
    if (parser == NULL || opt_name == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (strlen(opt_name) > MAX_ARG_NAME - 1) {
        errno = EINVAL;
        loge("argparser: Option name '%s' too long\n", opt_name);
        return NULL;
    }

    argparser_args_t arg;
    strcpy(arg.name, opt_name);
    arg.id = 0;
    arg.n = 0;

    que_elm_t *elm = NULL;
    argparser_args_t *ret = NULL;
    QUE_LOCK(parser->opt_names);
    if ((elm=QUE_FIND(parser->opt_names, &arg, sizeof(argparser_args_t), argcmp)) != NULL) {
        ret = QUE_ELM_DATA(elm, argparser_args_t);
    }
    QUE_UNLOCK(parser->opt_names);
    return ret;
}

/**
 * @brief   注册一个选项以及指定它带的参数个数
 * @param   parser      参数解析器
 * @param   opt_name    要注册的选项名称
 * @param   opt_id      选项ID
 * @param   param_num   选项所需的最少参数，如果命令行里该选项的参数少于param_num则报错
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
int argparser_add(argparser_t *parser, const char* opt_name, long opt_id, int param_num)
{
    if (parser == NULL || opt_name == NULL || opt_id <= 0 || param_num < 0) {
        errno = EINVAL;
        return -1;
    }
    if (strlen(opt_name) > MAX_ARG_NAME - 1) {
        errno = EINVAL;
        loge("argparser: Option name '%s' too long\n", opt_name);
        return -1;
    }

    argparser_args_t arg;
    strcpy(arg.name, opt_name);
    arg.id = opt_id;
    arg.n = param_num;
    QUE_LOCK(parser->opt_names);
    if (argparser_exist(parser, opt_name) == NULL) {
        if (que_insert_head(parser->opt_names, &arg, sizeof(argparser_args_t)) == 0) {
            QUE_LOCK(parser->opt_names);
            return 0;
        }
    } else {
        loge("argparser: Multiple option name '%s'\n", opt_name);
    }
    QUE_LOCK(parser->opt_names);

    return -1;
}

/**
 * @brief   开始解析
 * @param   parser      参数解析器
 * @param   parse_proc  解析器每发现一个选项，将回调处理函数parse_proc
 * @return  成功返回0，失败返回-1并设置errno
 */
int argparser_parse(argparser_t *parser, parse_callback_t parse_proc)
{
    if (parser == NULL || parse_proc == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    int prev_c = 0;
    char **prev_v = NULL, **param = NULL;
    argparser_args_t *arg_tmp = NULL;
    argparser_args_t *arg_cur = NULL;
    argparser_args_t *arg_prev = NULL;

    int c = parser->argc - 1;
    char **v = parser->argv + 1;
    QUE_LOCK(parser->opt_names);
    while (c >= 0) {
        if (c == 0 || (arg_tmp = argparser_exist(parser, *v)) != NULL) {
            arg_prev = arg_cur;
            arg_cur = arg_tmp;
            if (arg_prev != NULL) {
                if (prev_c < arg_prev->n) {
                    loge("argparser: Fail to parse '%s', Short of parameter\n", *prev_v);
                    QUE_UNLOCK(parser->opt_names);
                    return -1;
                }
                if (prev_c == 0)
                    param = NULL;
                else 
                    param = prev_v+1;
                /* number of arg may be more than 'arg->n' */
                if (parse_proc(arg_prev->id, param, prev_c) < 0) {  
                    QUE_UNLOCK(parser->opt_names);
                    return 0;   /* user stop parse */
                }
            }
            prev_c = 0;
            prev_v = v;
        } else {
            prev_c++;
        }
        c--;
        v++;
    }
    QUE_UNLOCK(parser->opt_names);

    return 0;
}

#ifdef __cplusplus
}
#endif

