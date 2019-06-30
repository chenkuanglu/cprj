/**
 * @file    common.c
 * @author  ln
 * @brief   进程信号同步处理、进程安全退出、基本模块初始化
 */

#include "common.h"
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    thrq_cb_t   thrq_start;     ///< 主线程消息队列
    pthread_t   tid_start;      ///< 主线程ID
    pthread_t   tid_sig;        ///< 进程同步信号处理线程的ID
} common_info_t;

static common_info_t cinfo;
static sigset_t mask_set;
static void* thread_sig(void *arg);

/**
 * @brief   基本初始化
 * @return  成功返回0，失败返回-1并设置errno
 */
int common_init(void)
{
    memset(&cinfo, 0, sizeof(common_info_t));
    if (err_init() != 0)
        return -1;
    if (thrq_init(&cinfo.thrq_start, NULL) != 0)
        return -1;

    // all sub threads mask signal SIGINT(2) & SIGTERM(15)
    sigemptyset(&mask_set);
    sigaddset(&mask_set, SIGINT);
    sigaddset(&mask_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask_set, NULL);
    // create signal query thread
    if (pthread_create(&cinfo.tid_sig, NULL, thread_sig, &cinfo) != 0)
        return -1;

    TMR_START();
    return 0;
}

/**
 * @brief   进程退出处理，与匹配common_init使用
 * @param   argc    命令行参数个数
 * @param   argv    命令行参数
 *
 * @return  成功返回0，失败返回-1并设置errno
 */
void common_stop(void)
{
    TMR_STOP();
}

/**
 * @brief   app退出函数（app退出时必须适时地调用一次common_stop）\n
 *          如果app没有定义app_proper_exit，则进程在退出时默认调用本函数
 * @param   ec      进程退出码，用于exit(ec)
 * @return  void
 */
__attribute__((weak))
void app_proper_exit(int ec)
{
    // app_stop1(...);
    common_stop();
    // app_stop2(...);
}

/**
 * @brief   app的main函数应该最终调用本函数，即等待退出消息
 * @param   ec      进程退出码，用于exit(ec)
 * @return  void
 */
int common_wait_exit(void)
{
    int ec;
    char ebuf[128];
    cinfo.tid_start = pthread_self();

    for (;;) {
        if (thrq_receive(&cinfo.thrq_start, &ec, sizeof(ec), 0) < 0) {
            loge("common_wait_exit() fail: %s\n", err_string(errno, ebuf, sizeof(ebuf)));
            nsleep(0.1);
            continue;
        }
        app_proper_exit(ec);
        exit(ec);
    }

    return ec;
}

/**
 * @brief   app需要退出时调用此函数，调用线程将立即被永久阻塞
 * @param   ec      进程退出码，用于exit(ec)
 * @return  void
 *
 * @attention   main线程禁止调用本函数，否则程序异常\n
 *              如果main主线程需要退出，直接return或者exit()
 */
void common_exit(int ec)
{
    thrq_send(&cinfo.thrq_start, &ec, sizeof(ec));
    COMMON_RETIRE();
}

// 进程的同步信号处理线程，信号SIGTERM & SIGINT只能由该线程处理
static void* thread_sig(void *arg)
{
    (void)arg;
    int sig, rc;
    sigset_t sig_set;
    char reason[64] = {0};

    pthread_t tid = pthread_self();
    pthread_detach(tid);

    sigemptyset(&sig_set);
    sigaddset(&sig_set, SIGTERM);
    sigaddset(&sig_set, SIGINT);

    for (;;) {
        rc = sigwait(&sig_set, &sig);
        if (rc == 0) {
            printf("\n");   // ^C
            sprintf(reason, "killed by signal '%s'", (sig == SIGTERM) ? "SIGTERM" : "SIGINT");
            logn("exit, %s\n", reason);
            common_exit(0);
        } else {
            loge("sigwait() fail\n");
            nsleep(0.1);
        }
    }
}

#ifdef __cplusplus
}
#endif
