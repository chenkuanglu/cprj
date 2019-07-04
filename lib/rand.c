/**
 * @file    rand.c
 * @author  ln
 * @brief   依赖指定文件和数据产生(sha256)随机种子、提供线程安全的随机数\n
 *          模块使用srandom_r和random_r产生线程安全的随机数
 *          rand()函数应用于简单场景，random()函数随机效果更好
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "rand.h"
#include "sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   依赖指定的文件、数据块和当前时间(timespec)来产生(sha256)随机种子\n
 *          如果文件较大，则最多读取1M=1024*1024的起始内容参与运算\n
 *          即使文件和数据都是NULL（甚至内部malloc失败），函数依然能够
 *          根据当前的（纳秒）时间通过sha256运算来生成不同的种子
 *
 * @param   seed    内存数据块，参与sha256运算
 *          len     内存数据长度
 *          file    文件名，文件内容参与sha256运算
 *
 * @return      始终返回随机种子
 *
 * @attention   内部有文件读取和动态内存分配(malloc)的操作
 * @code
 * unsigned int seed = rnd_genseed(&ip, sizeof(ip), "myconfig.conf");
 * @endcode
 */
unsigned int rnd_genseed(const void *seed, size_t len, const char *file)
{
    uint32_t hashout[8];
    struct timespec tms;
    struct stat fstat;
    size_t file_size = 0;

    if (file != NULL) {
        stat(file, &fstat);
        file_size = fstat.st_size % 1024*1024;      // file_size < 1M
    }
    if (seed == NULL || len == 0) {
        seed = NULL;
        len = 64;                                   // use stack data
    }

    unsigned buf_size = len + sizeof(struct timespec) + file_size;
    char *buf = (char*)malloc(buf_size);
    if (buf) {
        clock_gettime(CLOCK_MONOTONIC, &tms);
        memcpy(buf, &tms, sizeof(struct timespec));             // copy time
        if (seed) {
            memcpy(buf + sizeof(struct timespec), seed, len);   // copy memory block
        }
        if (file_size) {
            FILE *fp = fopen(file, "rb");
            if (fp) {                                           // copy file
                fread(buf + len + sizeof(struct timespec), 1, file_size, fp);
                fclose(fp);
            }
        }
        sha256(hashout, buf, buf_size);                         // generate seed
        free(buf);
    } else {
        sha256(hashout, &tms, sizeof(struct timespec));         // generate seed
    }

    return *((unsigned int *)hashout);
}

/**
 * @brief   初始化随机算法
 *
 * @param   seed    随机算法种子
 *          buf     随机算法的缓存，每个线程应该独立保留一份该缓存来保证线程安全
 *
 * @return  成功返回0，失败返回-1并设置errno
 * @code
 * unsigned int seed = rnd_genseed(&ip, sizeof(ip), NULL);
 * rnd_buf_t buf;
 * rnd_srandom(seed, &buf);
 * @endcode
 */
int rnd_srandom(unsigned int seed, rnd_buf_t *buf)
{
    return srandom_r(seed, buf);
}

/**
 * @brief   生成随机数
 *
 * @param   buf     已经由rnd_srandom初始化过的buffer
 *          result  输出随机值
 *
 * @attention   成功返回0，失败返回-1并设置errno
 * @code
 * unsigned int seed = rnd_genseed(&ip, sizeof(ip), NULL);
 * rnd_buf_t buf;
 * rnd_srandom(seed, &buf);
 *
 * int32_t value;
 * for (int i=0; i<10; i++) {
 *     rnd_random(&buf, &value);
 *     printf("random %d: %d", i, value);
 * }
 * @endcode
 */
int rnd_random(rnd_buf_t *buf, int32_t *result)
{
    return random_r(buf, result);
}

#ifdef __cplusplus
}
#endif

