/**
 * @file    rand.c
 * @author  ln
 * @brief   依赖指定文件和数据产生(sha256)随机种子、提供线程安全的随机数\n
 *          模块使用srandom_r和random_r产生线程安全的随机数
 *          rand()函数应用于简单场景，random()函数随机效果更好
 */

#ifndef __RAND_X__
#define __RAND_X__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct random_data rnd_buf_t;   ///< 随机算法的缓存数据类型

extern unsigned int rnd_genseed(const void *seed, size_t len, const char *file);
extern int rnd_srandom(unsigned int seed, rnd_buf_t *buf);
extern int rnd_random(rnd_buf_t *buf, int32_t *result);

#ifdef __cplusplus
}
#endif

#endif

