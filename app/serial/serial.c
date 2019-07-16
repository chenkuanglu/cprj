/**
 * @file    template.c
 * @author  ln
 * @brief   app 模板工程: applications
 */
 
#include "../../tty/tty.h"
#include "../../common/common.h"
#include "serial.h"
#include <sys/sysinfo.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *program_name = MAKE_CSTR(PROGRAM_NAME);     // 程序名称
argparser_t *cmdl_psr = NULL;       // 命令行解析器
pid_t pid_app = 0;                  // 进程ID
pthread_t tid_init = 0;             // 初始化线程的ID
double tm_startup = 0;              // 程序启动时刻

// serial
int fd = -1;

// 帮助信息
const char *usage = "\
Usage: " MAKE_CSTR(PROGRAM_NAME) " [Options] [Args]\n\
Options:\n\
    -v  --version   打印软件版本号\n\
    -h              打印帮助信息\n\
";

static int cmdline_proc(long id, char **param, int num);

void print_sysinfo(void)
{
    struct sysinfo sysi;
    sysinfo(&sysi);
    logn("system uptime: %02ld:%02ld:%02lds\n", 
            sysi.uptime/3600, (sysi.uptime%3600)/60, (sysi.uptime%3600)%60);
    logn("system free memory: " CCL_GREEN "%.3fMiB" CCL_WHITE_HL " / %.3fMiB\n" CCL_END, 
            (double)sysi.freeram/1024/1024, (double)sysi.totalram/1024/1024);
}

// app example : print 'hello world'
void event_write(void *arg)
{
    char buf[4] = {0x00, 0x00, 0x2a, 0x24};
    tty_write(fd, buf, sizeof(buf));
}

// thread for init 
void* thread_init(void *arg)
{
    char err_buf[128];
    
    // save the time of startup
    tm_startup = monotime();
    
    // save pid & tid
    pid_app = getpid();
    tid_init = pthread_self();
    
    // parse command line 
    if ((cmdl_psr = argparser_new(args_argc, args_argv)) == NULL) {
        loge("parse fail: %s\n", err_string(errno, err_buf, sizeof(err_buf)));
        common_exit(EXIT_SUCCESS);
    }     
    argparser_add(cmdl_psr, "--version", 'v', 0);
    argparser_add(cmdl_psr, "-v", 'v', 0);   
    argparser_add(cmdl_psr, "-h", 'h', 0);
    if (argparser_parse(cmdl_psr, cmdline_proc) != 0) {
        loge("parse fail: %s\n", err_string(errno, err_buf, sizeof(err_buf)));
        common_exit(EXIT_SUCCESS);
    }
    
    // print version
    logn("%s version %d.%d.%d\n", program_name, 
                    VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);

    // printf system info
    print_sysinfo();

    // create template thread
    void* thread_template(void *arg);
    pthread_t tid;
    pthread_create(&tid, 0, thread_template, 0);
    COMMON_RETIRE();
}

void* thread_template(void *arg)
{
    const char *dev = "/dev/ttyS1";
    fd = tty_open(dev, O_RDWR | O_NOCTTY, 115200);

    if (fd == -1) {
        loge("fail to open '%s'\n", dev);
        common_exit(EXIT_FAILURE);
    }

    TMR_ADD(100, TMR_EVENT_TYPE_PERIODIC, 3.0, event_write, NULL);    
    for (;;) {
        nsleep(0.2);
        if (monotime() - tm_startup >= 6) {
            logw("Time to exit.\n");
            common_exit(EXIT_SUCCESS);
        }
    }
}

// 退出回调函数，不要直接调用 
void app_proper_exit(int ec)
{
    long app_uptime = (long)(monotime() - tm_startup);
    logn("%s uptime: %02ld:%02ld:%02lds\n", 
            program_name, app_uptime/3600, (app_uptime%3600)/60, (app_uptime%3600)%60);
    print_sysinfo();

    // app example : remove timer event id=100
    TMR_REMOVE(100);
    
    // necessary
    common_stop();
    
    // app example 
    // ...
}

// 命令行参数的处理函数
static int cmdline_proc(long id, char **param, int num)
{
    switch (id) {
        case 'v':   // -v, --version
            printf("%s version %d.%d.%d\n", program_name, 
                    VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
            common_exit(EXIT_SUCCESS);
            break;
            
        case 'h':   // -h
            printf("%s\n", usage);
            common_exit(EXIT_SUCCESS);
            break;
            
        default:
            common_exit(EXIT_SUCCESS);
            break;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

