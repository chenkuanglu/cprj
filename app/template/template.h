/**
 * @file    template.h
 * @author  ln
 * @brief   app 模板工程: applications
 */

#ifndef __TEMPLATE_H__
#define __TEMPLATE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define PROGRAM_NAME    template    // 设置程序名称 

#define VERSION_MAJOR           0   // 主版本号
#define VERSION_MINOR           1   // 次版本号
#define VERSION_REVISION        0   // 修正版本号

extern double       tm_startup;
extern const char  *program_name;
extern const char  *usage;

extern int          args_argc;
extern char       **args_argv;

extern void     app_proper_exit(int ec);
extern void*    thread_init(void *arg);

#ifdef __cplusplus
}
#endif

#endif 
