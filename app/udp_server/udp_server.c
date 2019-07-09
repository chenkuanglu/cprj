/**
 * @file    template.c
 * @author  ln
 * @brief   app 模板工程: applications
 */
 
#include "../../common/common.h"
#include "udp_server.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *program_name = MAKE_CSTR(PROGRAM_NAME);     // 程序名称
argparser_t *cmdl_psr = NULL;       // 命令行解析器
pid_t pid_app = 0;                  // 进程ID
pthread_t tid_init = 0;             // 初始化线程的ID
double tm_startup = 0;              // 程序启动时刻

// 帮助信息
const char *usage = "\
Usage: " MAKE_CSTR(PROGRAM_NAME) " [Options] [Args]\n\
Options:\n\
    -v  --version   打印软件版本号\n\
    -h              打印帮助信息\n\
";

static int cmdline_proc(long id, char **param, int num);
static void* thread_udp_server(void *arg);

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

    // create server thread
    pthread_t tid;
    pthread_create(&tid, 0, thread_udp_server, 0);
    COMMON_RETIRE();
}

static void* thread_udp_server(void *arg)
{
    char buf[100] = {0};
    const char *sinfo = "{\"result\": true}";
    struct sockaddr_in src_addr;

    int fd = udp_open(0, 0, 0);
    udp_bind(fd, INADDR_ANY, 5500);
    for (;;) {
        if (udp_read(fd, buf, sizeof(buf), 0, &src_addr) < 0) {
            loge("socket recv fail: %s", strerror(errno));
            nsleep(0.1);
            continue;
        }
        logw("server read from '%s:%d': %s\n", inet_ntoa(NET_SOCKADDR(src_addr)), NET_SOCKPORT(src_addr), (char*)buf);

        logw("server write to  '%s:%d': %s\n\n", inet_ntoa(NET_SOCKADDR(src_addr)), NET_SOCKPORT(src_addr), (char*)sinfo);
        udp_write(fd, sinfo, strlen(sinfo), 0, &src_addr);
    }
}

// 退出回调函数，不要直接调用 
void app_proper_exit(int ec)
{
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

