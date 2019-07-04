/**
 * @file    rand.h
 * @author  ln
 * @brief   random 
 */

#ifndef __RND_X__
#define __RND_X__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct random_data rnd_buf_t;

extern unsigned int rnd_genseed(const void *seed, size_t len, const char *file);
extern int rnd_srandom(unsigned int seed, rnd_buf_t *buf);
extern int rnd_random(rnd_buf_t *buf, int32_t *result);

#ifdef __cplusplus
}
#endif

#endif // __THR_MUX__

